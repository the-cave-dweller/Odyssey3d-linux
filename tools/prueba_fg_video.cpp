/* Prueba: MAKO FG (mako-fg-host, protocolo 3) generando cuadros de un VIDEO, fuera de cualquier juego.
   Lo mismo que hace la capa de MAKO (engine/mako-backend/src/mako_fg.cpp y swapchain/resources.cpp), sin MAKO:
     - arranca mako-fg-host con su socket en el fd 3; Hello (protocolo 3) y Devices
     - 2 imagenes de entrada y N de salida RGBA8 optimas con memoria dedicada exportada (fd opaco) y un semaforo de linea
       de tiempo exportado; Open los manda por el socket (SCM_RIGHTS)
     - por cada cuadro real: se sube a la entrada b%2 (adquirir de EXTERNAL, copiar, soltar a EXTERNAL) señalando v; Schedule
       con los tiempos i/(N+1); el host espera v y señala v+1..v+N; el siguiente real es v+N+1
   Uso: prueba_fg_video RAWDIR CUADROS ANCHO ALTO MULT FLUJO SALIDA HOST [GUARDAR_DESDE GUARDAR_CUANTOS] */
#include <vulkan/vulkan.h>

#include <spawn.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern char **environ;

namespace {
constexpr uint32_t maxOutputs = 32;
enum Op : uint32_t { Hello = 1, Devices, Supports, Open, Schedule, Ready, EnableProfiling, ReadProfile };
struct alignas(8) Request {
  uint32_t op, count;
  uint64_t timeout;
  int32_t device, encoding, period;
  uint32_t half, width, height;
  float flow, times[maxOutputs], interval;
};
struct Reply {
  int32_t failed;
  uint32_t first, second;
  char text[4084];
};
struct GpuInfo {
  int32_t index;
  uint32_t vendor, id;
  char pci[32], name[256];
  uint8_t driverUuid[16], deviceUuid[16];
};
static_assert(sizeof(Request) == 176);

[[noreturn]] void muere(const std::string &m) {
  std::fprintf(stderr, "ERROR: %s\n", m.c_str());
  std::exit(1);
}
#define VK(x)                                                                         \
  do {                                                                                \
    VkResult r_ = (x);                                                                \
    if (r_ != VK_SUCCESS) muere(std::string(#x) + " = " + std::to_string((int)r_)); \
  } while (0)

double ahora() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool pide(int s, const Request &q, Reply &r) {
  if (send(s, &q, sizeof q, MSG_NOSIGNAL) != (ssize_t)sizeof q) return false;
  ssize_t n;
  do n = recv(s, &r, sizeof r, 0);
  while (n < 0 && errno == EINTR);
  return n >= (ssize_t)offsetof(Reply, text);
}

struct Vk {
  VkInstance inst;
  VkPhysicalDevice fis;
  VkDevice dev;
  uint32_t familia;
  VkQueue cola;
  VkCommandPool pool;
  PFN_vkGetMemoryFdKHR getMemFd;
  PFN_vkGetSemaphoreFdKHR getSemFd;
  uint8_t uuid[16];
};

uint32_t tipoMemoria(Vk &v, uint32_t bits, VkMemoryPropertyFlags quiero) {
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(v.fis, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & quiero) == quiero) return i;
  muere("sin tipo de memoria");
}

struct Img {
  VkImage img;
  VkDeviceMemory mem;
  int fd;
};
Img imagenExportada(Vk &v, uint32_t w, uint32_t h, VkImageUsageFlags uso) {
  Img r{};
  VkExternalMemoryImageCreateInfo ext{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
  ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ci.pNext = &ext, ci.imageType = VK_IMAGE_TYPE_2D, ci.format = VK_FORMAT_R8G8B8A8_UNORM, ci.extent = {w, h, 1};
  ci.mipLevels = 1, ci.arrayLayers = 1, ci.samples = VK_SAMPLE_COUNT_1_BIT, ci.usage = uso;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VK(vkCreateImage(v.dev, &ci, nullptr, &r.img));
  VkMemoryRequirements mr;
  vkGetImageMemoryRequirements(v.dev, r.img, &mr);
  VkMemoryDedicatedAllocateInfo ded{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
  ded.image = r.img;
  VkExportMemoryAllocateInfo exp{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
  exp.pNext = &ded, exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.pNext = &exp, ai.allocationSize = mr.size, ai.memoryTypeIndex = tipoMemoria(v, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK(vkAllocateMemory(v.dev, &ai, nullptr, &r.mem));
  VK(vkBindImageMemory(v.dev, r.img, r.mem, 0));
  VkMemoryGetFdInfoKHR gi{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
  gi.memory = r.mem, gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VK(v.getMemFd(v.dev, &gi, &r.fd));
  return r;
}

struct Buf {
  VkBuffer b;
  VkDeviceMemory m;
  void *p;
};
Buf bufer(Vk &v, VkDeviceSize n) {
  Buf r{};
  VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  ci.size = n, ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VK(vkCreateBuffer(v.dev, &ci, nullptr, &r.b));
  VkMemoryRequirements mr;
  vkGetBufferMemoryRequirements(v.dev, r.b, &mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(v, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK(vkAllocateMemory(v.dev, &ai, nullptr, &r.m));
  VK(vkBindBufferMemory(v.dev, r.b, r.m, 0));
  VK(vkMapMemory(v.dev, r.m, 0, n, 0, &r.p));
  return r;
}

VkImageMemoryBarrier barrera(VkImage img, uint32_t de, uint32_t a, VkAccessFlags src, VkAccessFlags dst) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask = src, b.dstAccessMask = dst, b.oldLayout = VK_IMAGE_LAYOUT_GENERAL, b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  b.srcQueueFamilyIndex = de, b.dstQueueFamilyIndex = a, b.image = img, b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  return b;
}

/* buffer -> imagen (subir) o imagen -> buffer (leer), con la imagen adquirida de EXTERNAL y devuelta; señala `senal` */
void copiar(Vk &v, VkCommandBuffer cb, VkSemaphore tl, uint64_t espera, uint64_t senal, Img &im, Buf &bf, uint32_t w, uint32_t h,
            bool subir) {
  VK(vkResetCommandBuffer(cb, 0));
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK(vkBeginCommandBuffer(cb, &bi));
  const VkAccessFlags acc = subir ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_TRANSFER_READ_BIT;
  VkImageMemoryBarrier in = barrera(im.img, VK_QUEUE_FAMILY_EXTERNAL, v.familia, 0, acc);
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &in);
  VkBufferImageCopy c{};
  c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  c.imageExtent = {w, h, 1};
  if (subir) vkCmdCopyBufferToImage(cb, bf.b, im.img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
  else vkCmdCopyImageToBuffer(cb, im.img, VK_IMAGE_LAYOUT_GENERAL, bf.b, 1, &c);
  VkImageMemoryBarrier out = barrera(im.img, v.familia, VK_QUEUE_FAMILY_EXTERNAL, acc, 0);
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &out);
  VK(vkEndCommandBuffer(cb));
  VkTimelineSemaphoreSubmitInfo ts{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
  ts.waitSemaphoreValueCount = espera ? 1 : 0, ts.pWaitSemaphoreValues = &espera;
  ts.signalSemaphoreValueCount = senal ? 1 : 0, ts.pSignalSemaphoreValues = &senal;
  const VkPipelineStageFlags st = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.pNext = &ts, si.commandBufferCount = 1, si.pCommandBuffers = &cb;
  si.waitSemaphoreCount = espera ? 1 : 0, si.pWaitSemaphores = &tl, si.pWaitDstStageMask = &st;
  si.signalSemaphoreCount = senal ? 1 : 0, si.pSignalSemaphores = &tl;
  VK(vkQueueSubmit(v.cola, 1, &si, VK_NULL_HANDLE));
}

void esperar(Vk &v, VkSemaphore tl, uint64_t valor) {
  VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
  wi.semaphoreCount = 1, wi.pSemaphores = &tl, wi.pValues = &valor;
  VK(vkWaitSemaphores(v.dev, &wi, 5000000000ull));
}
}  // namespace

int main(int argc, char **argv) {
  if (argc < 9) muere("uso: RAWDIR CUADROS ANCHO ALTO MULT FLUJO SALIDA HOST [DESDE CUANTOS]");
  const std::string raw = argv[1], salida = argv[7], hostRuta = argv[8];
  const int cuadros = std::atoi(argv[2]);
  const uint32_t W = (uint32_t)std::atoi(argv[3]), H = (uint32_t)std::atoi(argv[4]), N = (uint32_t)std::atoi(argv[5]) - 1;
  const float flujo = (float)std::atof(argv[6]);
  const int desde = argc > 9 ? std::atoi(argv[9]) : 10, cuantos = argc > 10 ? std::atoi(argv[10]) : 2;
  const char *quiero = std::getenv("TARJETA") ? std::getenv("TARJETA") : "4090";

  /* Vulkan */
  Vk v{};
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  VK(vkCreateInstance(&ici, nullptr, &v.inst));
  uint32_t nf = 0;
  vkEnumeratePhysicalDevices(v.inst, &nf, nullptr);
  std::vector<VkPhysicalDevice> fs(nf);
  vkEnumeratePhysicalDevices(v.inst, &nf, fs.data());
  for (VkPhysicalDevice f : fs) {
    VkPhysicalDeviceIDProperties idp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &idp;
    vkGetPhysicalDeviceProperties2(f, &p2);
    if (std::strstr(p2.properties.deviceName, quiero)) {
      v.fis = f;
      std::memcpy(v.uuid, idp.deviceUUID, 16);
      std::printf("tarjeta: %s\n", p2.properties.deviceName);
    }
  }
  if (!v.fis) muere("no esta la tarjeta");
  uint32_t nq = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(v.fis, &nq, nullptr);
  std::vector<VkQueueFamilyProperties> qs(nq);
  vkGetPhysicalDeviceQueueFamilyProperties(v.fis, &nq, qs.data());
  for (uint32_t i = 0; i < nq; i++)
    if (qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { v.familia = i; break; }
  const float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = v.familia, qci.queueCount = 1, qci.pQueuePriorities = &prio;
  const char *exts[] = {VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
  VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  f12.timelineSemaphore = VK_TRUE;
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.pNext = &f12, dci.queueCreateInfoCount = 1, dci.pQueueCreateInfos = &qci, dci.enabledExtensionCount = 2, dci.ppEnabledExtensionNames = exts;
  VK(vkCreateDevice(v.fis, &dci, nullptr, &v.dev));
  vkGetDeviceQueue(v.dev, v.familia, 0, &v.cola);
  v.getMemFd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(v.dev, "vkGetMemoryFdKHR");
  v.getSemFd = (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(v.dev, "vkGetSemaphoreFdKHR");
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, pci.queueFamilyIndex = v.familia;
  VK(vkCreateCommandPool(v.dev, &pci, nullptr, &v.pool));

  /* el host */
  int sk[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sk) != 0) muere("socketpair");
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, sk[1], 3);
  pid_t hp = 0;
  char *hargv[] = {(char *)hostRuta.c_str(), nullptr};
  if (posix_spawn(&hp, hostRuta.c_str(), &fa, nullptr, hargv, environ) != 0) muere("no arranco el host");
  close(sk[1]);
  int st = 0;
  waitpid(hp, &st, 0);   /* su primer proceso sale enseguida (mako_fg.cpp) */
  const int ctl = sk[0];
  timeval lim{120, 0};
  setsockopt(ctl, SOL_SOCKET, SO_RCVTIMEO, &lim, sizeof lim);
  Request q{};
  Reply r{};
  q.op = Hello;
  if (!pide(ctl, q, r) || r.failed) muere("Hello");
  std::printf("host: protocolo %u, peticion %u bytes, modelo «%s»\n", r.first, r.second, r.text);
  if (r.first != 3 || r.second != sizeof(Request)) muere("no es el protocolo 3");
  q = {};
  q.op = Devices;
  if (!pide(ctl, q, r) || r.failed) muere("Devices");
  int indice = -1;
  for (uint32_t i = 0; i < r.first && i < 8; i++) {
    GpuInfo g;
    std::memcpy(&g, r.text + i * sizeof(GpuInfo), sizeof g);
    std::printf("  host ve: %d %s %s\n", g.index, g.name, g.pci);
    if (std::memcmp(g.deviceUuid, v.uuid, 16) == 0) indice = g.index;
  }
  if (indice < 0) muere("el host no ve nuestra tarjeta");

  /* las imagenes y el semaforo */
  std::vector<Img> fuente, destino;
  for (int i = 0; i < 2; i++) fuente.push_back(imagenExportada(v, W, H, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
  for (uint32_t i = 0; i < N; i++) destino.push_back(imagenExportada(v, W, H, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
  VkExportSemaphoreCreateInfo esc{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
  esc.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkSemaphoreTypeCreateInfo stc{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
  stc.pNext = &esc, stc.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, stc.initialValue = 0;
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  sci.pNext = &stc;
  VkSemaphore tl;
  VK(vkCreateSemaphore(v.dev, &sci, nullptr, &tl));
  VkSemaphoreGetFdInfoKHR sgi{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
  sgi.semaphore = tl, sgi.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
  int tlFd = -1;
  VK(v.getSemFd(v.dev, &sgi, &tlFd));

  /* Open */
  int cs[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, cs) != 0) muere("socketpair ctx");
  std::vector<int> fds = {cs[1], fuente[0].fd, fuente[1].fd, tlFd};
  for (Img &d : destino) fds.push_back(d.fd);
  Request op{};
  op.op = Open, op.count = N, op.device = indice, op.encoding = 0, op.period = 2, op.half = 1, op.width = W, op.height = H, op.flow = flujo;
  alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * (maxOutputs + 4))> espacio{};
  iovec parte{&op, sizeof op};
  msghdr msg{};
  msg.msg_iov = &parte, msg.msg_iovlen = 1, msg.msg_control = espacio.data(), msg.msg_controllen = CMSG_SPACE(sizeof(int) * fds.size());
  cmsghdr *cm = CMSG_FIRSTHDR(&msg);
  cm->cmsg_level = SOL_SOCKET, cm->cmsg_type = SCM_RIGHTS, cm->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
  std::memcpy(CMSG_DATA(cm), fds.data(), sizeof(int) * fds.size());
  if (sendmsg(ctl, &msg, MSG_NOSIGNAL) != (ssize_t)sizeof op) muere("Open no salio");
  for (int fd : fds) close(fd);
  const int ctx = cs[0];
  setsockopt(ctx, SOL_SOCKET, SO_RCVTIMEO, &lim, sizeof lim);
  ssize_t n;
  do n = recv(ctx, &r, sizeof r, 0);
  while (n < 0 && errno == EINTR);
  if (n < (ssize_t)offsetof(Reply, text)) muere("Open sin respuesta");
  if (r.failed) muere(std::string("Open: ") + r.text);
  std::printf("contexto abierto: %ux%u, %u generados por real, analisis %ux%u, flujo %.2f\n", W, H, N, r.first, r.second, flujo);
  timeval lim3{3, 0};
  setsockopt(ctx, SOL_SOCKET, SO_RCVTIMEO, &lim3, sizeof lim3);

  /* los cuadros */
  const VkDeviceSize tam = (VkDeviceSize)W * H * 4;
  Buf sube = bufer(v, tam), lee = bufer(v, tam);
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = v.pool, cai.commandBufferCount = 1;
  VkCommandBuffer cb;
  VK(vkAllocateCommandBuffers(v.dev, &cai, &cb));
  uint64_t valor = 1;
  std::vector<double> lat;
  const double intervalo = 1.0 / 24.0;
  for (int b = 0; b < cuadros; b++) {
    char nom[512];
    std::snprintf(nom, sizeof nom, "%s/%03d.rgba", raw.c_str(), b + 1);
    std::ifstream f(nom, std::ios::binary);
    if (!f) muere(std::string("falta ") + nom);
    f.read((char *)sube.p, (std::streamsize)tam);
    const double t0 = ahora();
    copiar(v, cb, tl, valor > 1 ? valor - 1 : 0, valor, fuente[b % 2], sube, W, H, true);
    Request s{};
    s.op = Schedule;
    if (b == 0) {
      s.count = 0, s.interval = (float)intervalo;   /* el primero solo arranca la historia */
    } else {
      s.count = N, s.interval = (float)intervalo;
      for (uint32_t i = 0; i < N; i++) s.times[i] = (float)(i + 1) / (float)(N + 1);
    }
    if (!pide(ctx, s, r) || r.failed) muere(std::string("Schedule: ") + (r.failed ? r.text : "sin respuesta"));
    const uint64_t fin = b == 0 ? valor : valor + N;
    esperar(v, tl, fin);
    const double t1 = ahora();
    if (b > 0) lat.push_back((t1 - t0) * 1000.0);
    if (b >= desde && b < desde + cuantos)
      for (uint32_t i = 0; i < N; i++) {
        vkQueueWaitIdle(v.cola);
        copiar(v, cb, tl, 0, 0, destino[i], lee, W, H, false);
        vkQueueWaitIdle(v.cola);
        std::snprintf(nom, sizeof nom, "%s/gen-%03d-%u.rgba", salida.c_str(), b + 1, i + 1);
        std::ofstream o(nom, std::ios::binary);
        o.write((const char *)lee.p, (std::streamsize)tam);
      }
    valor = fin + 1;
  }
  std::sort(lat.begin(), lat.end());
  double suma = 0;
  for (double x : lat) suma += x;
  std::printf("%zu lotes: subir + generar %u cuadros: media %.2f ms, mediana %.2f, p95 %.2f, max %.2f\n", lat.size(), N,
              suma / (double)lat.size(), lat[lat.size() / 2], lat[lat.size() * 95 / 100], lat.back());
  /* adios: el host acaba cuando se cierran los sockets */
  shutdown(ctx, SHUT_WR);
  char c;
  recv(ctx, &c, 1, 0);
  close(ctx);
  close(ctl);
  vkDeviceWaitIdle(v.dev);
  return 0;
}
