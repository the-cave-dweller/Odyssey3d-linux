/* fgvideo.cpp — ver fgvideo.h */
#include "fgvideo.h"

#include "mako.h"

#include <spawn.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern char **environ;

namespace {
/* el protocolo de mako-fg-host (engine/mako-backend/src/mako_fg_protocol.hpp, hostProtocol = 3) */
constexpr uint32_t kProtocolo = 3, kMaxSalidas = 32;
enum Op : uint32_t { Hello = 1, Devices, Supports, Open, Schedule, Ready, EnableProfiling, ReadProfile };
struct alignas(8) Peticion {
  uint32_t op, count;
  uint64_t timeout;
  int32_t device, encoding, period;
  uint32_t half, width, height;
  float flow, times[kMaxSalidas], interval;
};
struct Respuesta {
  int32_t failed;
  uint32_t first, second;
  char text[4084];
};
struct InfoGpu {
  int32_t index;
  uint32_t vendor, id;
  char pci[32], name[256];
  uint8_t driverUuid[16], deviceUuid[16];
};
static_assert(sizeof(Peticion) == 176, "el protocolo 3 de mako-fg-host");

bool recibir(int s, Respuesta &r) {
  ssize_t n;
  do n = recv(s, &r, sizeof r, 0);
  while (n < 0 && errno == EINTR);
  return n >= (ssize_t)offsetof(Respuesta, text);
}
bool pedir(int s, const Peticion &q, Respuesta &r) {
  return send(s, &q, sizeof q, MSG_NOSIGNAL) == (ssize_t)sizeof q && recibir(s, r);
}
void plazo(int s, int segundos) {
  const timeval t{segundos, 0};
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);
}

/* el programa: el de MAKO, el que instalo OdysseyVK o $MAKO_FG_HOST */
std::string programaHost() {
  if (const char *h = std::getenv("MAKO_FG_HOST"); h && *h) return h;
  const std::string s = mako::hostSistema();
  return s.empty() ? mako::hostPropio() : s;
}

uint32_t tipoMemoria(VkPhysicalDevice f, uint32_t bits, VkMemoryPropertyFlags quiero) {
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(f, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & quiero) == quiero) return i;
  return UINT32_MAX;
}

VkImageMemoryBarrier barrera(VkImage img, uint32_t de, uint32_t a, VkAccessFlags src, VkAccessFlags dst) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask = src, b.dstAccessMask = dst;
  b.oldLayout = b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  b.srcQueueFamilyIndex = de, b.dstQueueFamilyIndex = a, b.image = img;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  return b;
}
}  // namespace

bool FgVideo::crearExportada(Img &im, VkImageUsageFlags uso, int *fd, std::string *err) {
  const VkDevice d = dev_->device();
  VkExternalMemoryImageCreateInfo ext{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
  ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ci.pNext = &ext, ci.imageType = VK_IMAGE_TYPE_2D, ci.format = VK_FORMAT_R8G8B8A8_UNORM;
  ci.extent = {(uint32_t)w_, (uint32_t)h_, 1}, ci.mipLevels = 1, ci.arrayLayers = 1, ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.usage = uso, ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateImage(d, &ci, nullptr, &im.img) != VK_SUCCESS) return (err && (*err = "vkCreateImage (exportada)", true)), false;
  VkMemoryRequirements mr;
  vkGetImageMemoryRequirements(d, im.img, &mr);
  VkMemoryDedicatedAllocateInfo ded{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
  ded.image = im.img;
  VkExportMemoryAllocateInfo exp{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
  exp.pNext = &ded, exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.pNext = &exp, ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(dev_->phys(), mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(d, &ai, nullptr, &im.mem) != VK_SUCCESS ||
      vkBindImageMemory(d, im.img, im.mem, 0) != VK_SUCCESS) {
    if (err) *err = "sin memoria para las imagenes de MAKO FG";
    return false;
  }
  auto getFd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(d, "vkGetMemoryFdKHR");
  VkMemoryGetFdInfoKHR gi{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
  gi.memory = im.mem, gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  if (!getFd || getFd(d, &gi, fd) != VK_SUCCESS) {
    if (err) *err = "no se pudo exportar la memoria (vkGetMemoryFdKHR)";
    return false;
  }
  return true;
}

bool FgVideo::crearAnillo(Img &im, std::string *err) {
  /* donde se copia cada generado para dibujarlo: B8G8R8A8 como la captura (los bytes viajan tal cual por las imagenes
     RGBA8 del host, asi que el orden de los colores es el de la captura) y en GENERAL, como la fuente de la trama */
  const VkDevice d = dev_->device();
  VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ci.imageType = VK_IMAGE_TYPE_2D, ci.format = formato_;
  ci.extent = {(uint32_t)w_, (uint32_t)h_, 1}, ci.mipLevels = 1, ci.arrayLayers = 1, ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateImage(d, &ci, nullptr, &im.img) != VK_SUCCESS) return (err && (*err = "vkCreateImage (anillo)", true)), false;
  VkMemoryRequirements mr;
  vkGetImageMemoryRequirements(d, im.img, &mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(dev_->phys(), mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(d, &ai, nullptr, &im.mem) != VK_SUCCESS ||
      vkBindImageMemory(d, im.img, im.mem, 0) != VK_SUCCESS) {
    if (err) *err = "sin memoria para el anillo de MAKO FG";
    return false;
  }
  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = im.img, vi.viewType = VK_IMAGE_VIEW_TYPE_2D, vi.format = ci.format;
  vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (vkCreateImageView(d, &vi, nullptr, &im.view) != VK_SUCCESS) return (err && (*err = "vkCreateImageView (anillo)", true)), false;
  return true;
}

void FgVideo::destruir(Img &im) {
  if (!dev_) return;
  const VkDevice d = dev_->device();
  if (im.view) vkDestroyImageView(d, im.view, nullptr);
  if (im.img) vkDestroyImage(d, im.img, nullptr);
  if (im.mem) vkFreeMemory(d, im.mem, nullptr);
  im = Img{};
}

void FgVideo::cerrar() {
  if (ctx_ >= 0) {
    /* el host cierra su lado cuando acaba lo que tenia en la tarjeta (mako_fg.cpp, ~FGContext) */
    shutdown(ctx_, SHUT_WR);
    plazo(ctx_, 2);
    char c;
    recv(ctx_, &c, 1, 0);
    close(ctx_);
    ctx_ = -1;
  }
  if (control_ >= 0) close(control_), control_ = -1;
  /* el destructor vuelve a cerrar al salir del visor, cuando el dispositivo ya no esta: solo se toca si sigue vivo, y
     despues se olvida (MEDIDO: vkDeviceWaitIdle con el dispositivo destruido abortaba al cerrar) */
  if (dev_ && dev_->ok()) {
    vkDeviceWaitIdle(dev_->device());
    for (Img &i : fuente_) destruir(i);
    for (Img &i : destino_) destruir(i);
    for (Img &i : anillo_) destruir(i);
    for (VkFence f : vallas_) vkDestroyFence(dev_->device(), f, nullptr);
    if (pool_) vkDestroyCommandPool(dev_->device(), pool_, nullptr);
    if (tl_) vkDestroySemaphore(dev_->device(), tl_, nullptr);
  }
  fuente_.clear(), destino_.clear(), anillo_.clear(), cbs_.clear(), vallas_.clear(), plan_.clear();
  pool_ = VK_NULL_HANDLE, tl_ = VK_NULL_HANDLE, ultimaVista_ = VK_NULL_HANDLE;
  valor_ = 1, ultimoFin_ = 0, lote_ = 0, w_ = h_ = 0, n_ = 0;
  /* las posiciones del anillo y de los buffers, a cero: al cambiar de multiplicador el anillo cambia de tamano. MEDIDO el
     27-nov: de x4 (10 imagenes) a x2 (4) se seguia escribiendo en la 7 y el visor se caia */
  anilloSig_ = 0, cbSig_ = 0;
  dev_ = nullptr;
}

bool FgVideo::preparar(VkDev &dev, int w, int h, int mult, VkFormat formato, std::string *err) {
  mult = std::max(2, std::min(4, mult));
  if (ctx_ >= 0 && dev_ == &dev && w == w_ && h == h_ && (uint32_t)mult - 1 == n_ && formato == formato_) return true;
  cerrar();
  dev_ = &dev, w_ = w, h_ = h, n_ = (uint32_t)mult - 1, formato_ = formato;
  const std::string prog = programaHost();
  if (prog.empty()) {
    if (err) *err = "MAKO FG no esta instalado (pestana FG)";
    return false;
  }
  /* el host, con su socket en el fd 3 (mako_fg.cpp: startHost). Su primer proceso sale enseguida */
  int sk[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sk) != 0) return (err && (*err = "socketpair", true)), false;
  std::vector<std::string> entorno;
  for (char **e = environ; e && *e; e++)
    if (std::strncmp(*e, "LD_PRELOAD=", 11) != 0) entorno.emplace_back(*e);
  std::vector<char *> envp;
  for (std::string &s : entorno) envp.push_back(s.data());
  envp.push_back(nullptr);
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, sk[1], 3);
  pid_t pid = 0;
  std::string p = prog;
  char *argv[] = {p.data(), nullptr};
  const int r0 = posix_spawn(&pid, prog.c_str(), &fa, nullptr, argv, envp.data());
  posix_spawn_file_actions_destroy(&fa);
  close(sk[1]);
  if (r0 != 0) {
    close(sk[0]);
    if (err) *err = "no arranco " + prog;
    return false;
  }
  int st = 0;
  while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
  }
  control_ = sk[0];
  plazo(control_, 60);
  auto falla = [&](const std::string &m) {
    if (err) *err = m;
    cerrar();
    return false;
  };
  Peticion q{};
  Respuesta r{};
  q.op = Hello;
  if (!pedir(control_, q, r) || r.failed) return falla("mako-fg-host no contesta");
  if (r.first != kProtocolo || r.second != sizeof(Peticion)) return falla("mako-fg-host habla otro protocolo");
  /* la tarjeta del visor, por su deviceUUID */
  VkPhysicalDeviceIDProperties idp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
  VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  p2.pNext = &idp;
  vkGetPhysicalDeviceProperties2(dev.phys(), &p2);
  q = {};
  q.op = Devices;
  if (!pedir(control_, q, r) || r.failed) return falla("mako-fg-host no da sus tarjetas");
  int indice = -1;
  for (uint32_t i = 0; i < r.first && i < 8; i++) {
    InfoGpu g;
    std::memcpy(&g, r.text + i * sizeof(InfoGpu), sizeof g);
    if (std::memcmp(g.deviceUuid, idp.deviceUUID, 16) == 0) indice = g.index;
  }
  if (indice < 0) return falla("mako-fg-host no ve la tarjeta del visor");
  /* las imagenes y el semaforo, exportados */
  const VkDevice d = dev.device();
  std::vector<int> fds;
  fuente_.resize(2), destino_.resize(n_);
  for (Img &i : fuente_) {
    int fd = -1;
    if (!crearExportada(i, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &fd, err)) return falla(err ? *err : "");
    fds.push_back(fd);
  }
  for (Img &i : destino_) {
    int fd = -1;
    if (!crearExportada(i, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &fd, err)) return falla(err ? *err : "");
    fds.push_back(fd);
  }
  VkExportSemaphoreCreateInfo esc{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
  esc.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkSemaphoreTypeCreateInfo stc{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
  stc.pNext = &esc, stc.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, stc.initialValue = 0;
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  sci.pNext = &stc;
  if (vkCreateSemaphore(d, &sci, nullptr, &tl_) != VK_SUCCESS) return falla("vkCreateSemaphore (linea de tiempo)");
  auto getSemFd = (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(d, "vkGetSemaphoreFdKHR");
  VkSemaphoreGetFdInfoKHR sgi{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
  sgi.semaphore = tl_, sgi.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
  int tlFd = -1;
  if (!getSemFd || getSemFd(d, &sgi, &tlFd) != VK_SUCCESS) return falla("no se pudo exportar el semaforo");
  /* Open, con los descriptores: [el socket del contexto, entrada A, entrada B, semaforo, salidas...] */
  int cs[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, cs) != 0) return falla("socketpair (contexto)");
  std::vector<int> todos = {cs[1], fds[0], fds[1], tlFd};
  for (size_t i = 2; i < fds.size(); i++) todos.push_back(fds[i]);
  Peticion op{};
  op.op = Open, op.count = n_, op.device = indice, op.encoding = 0 /* SDR 8 bits */, op.period = 2, op.half = 1;
  op.width = (uint32_t)w_, op.height = (uint32_t)h_, op.flow = 0.85f;
  alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int) * (kMaxSalidas + 4))> espacio{};
  iovec parte{&op, sizeof op};
  msghdr m{};
  m.msg_iov = &parte, m.msg_iovlen = 1, m.msg_control = espacio.data(), m.msg_controllen = CMSG_SPACE(sizeof(int) * todos.size());
  cmsghdr *cm = CMSG_FIRSTHDR(&m);
  cm->cmsg_level = SOL_SOCKET, cm->cmsg_type = SCM_RIGHTS, cm->cmsg_len = CMSG_LEN(sizeof(int) * todos.size());
  std::memcpy(CMSG_DATA(cm), todos.data(), sizeof(int) * todos.size());
  const bool salio = sendmsg(control_, &m, MSG_NOSIGNAL) == (ssize_t)sizeof op;
  for (int fd : todos) close(fd);   /* el host se queda con los suyos */
  if (!salio) {
    close(cs[0]);
    return falla("Open no llego a mako-fg-host");
  }
  ctx_ = cs[0];
  plazo(ctx_, 60);
  if (!recibir(ctx_, r)) return falla("mako-fg-host no abrio el contexto");
  if (r.failed) return falla(std::string("mako-fg-host: ") + r.text);
  plazo(ctx_, 3);
  /* el anillo: lo que puede estar en vuelo (tres cuadros del visor) por cada generado, y uno de holgura */
  anillo_.resize((size_t)n_ * 3 + 1);
  for (Img &i : anillo_)
    if (!crearAnillo(i, err)) return falla(err ? *err : "");
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, pci.queueFamilyIndex = dev.queueFamily();
  if (vkCreateCommandPool(d, &pci, nullptr, &pool_) != VK_SUCCESS) return falla("vkCreateCommandPool");
  cbs_.resize(8), vallas_.resize(8);
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = pool_, cai.commandBufferCount = (uint32_t)cbs_.size();
  if (vkAllocateCommandBuffers(d, &cai, cbs_.data()) != VK_SUCCESS) return falla("vkAllocateCommandBuffers");
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (VkFence &f : vallas_) vkCreateFence(d, &fci, nullptr, &f);
  /* el anillo, a GENERAL una vez */
  {
    VkCommandBuffer cb = cbs_[0];
    vkWaitForFences(d, 1, &vallas_[0], VK_TRUE, UINT64_MAX);
    vkResetFences(d, 1, &vallas_[0]);
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    std::vector<VkImageMemoryBarrier> bs;
    for (Img &i : anillo_) {
      VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, b.image = i.img;
      b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      bs.push_back(b);
    }
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                         (uint32_t)bs.size(), bs.data());
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1, si.pCommandBuffers = &cb;
    vkQueueSubmit(dev.queue(), 1, &si, vallas_[0]);
  }
  std::printf("[fg-video] MAKO FG (%s) abierto en la tarjeta %d: %dx%d, x%d, analisis %ux%u\n", prog.c_str(), indice, w_, h_,
              (int)n_ + 1, r.first, r.second);
  std::fflush(stdout);
  return true;
}

bool FgVideo::enviar(size_t k, uint64_t espera, uint64_t senal, std::string *err) {
  VkCommandBuffer cb = cbs_[k];
  VkTimelineSemaphoreSubmitInfo ts{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
  ts.waitSemaphoreValueCount = espera ? 1 : 0, ts.pWaitSemaphoreValues = &espera;
  ts.signalSemaphoreValueCount = senal ? 1 : 0, ts.pSignalSemaphoreValues = &senal;
  const VkPipelineStageFlags etapa = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.pNext = &ts, si.commandBufferCount = 1, si.pCommandBuffers = &cb;
  si.waitSemaphoreCount = espera ? 1 : 0, si.pWaitSemaphores = &tl_, si.pWaitDstStageMask = &etapa;
  si.signalSemaphoreCount = senal ? 1 : 0, si.pSignalSemaphores = &tl_;
  if (vkQueueSubmit(dev_->queue(), 1, &si, vallas_[k]) != VK_SUCCESS) {
    if (err) *err = "vkQueueSubmit";
    return false;
  }
  return true;
}

bool FgVideo::nuevoReal(const VkImg &src, double ahora, std::string *err) {
  if (ctx_ < 0 || !src.valid()) return false;
  if (src.w != w_ || src.h != h_) return true;   /* otro tamano: quien llama prepara otra vez */
  const VkDevice d = dev_->device();
  /* el ritmo del video: la media de los intervalos entre reales (entre 1/60 y 1/12 s) */
  if (ultimoReal_ > 0.0) {
    const double dt = ahora - ultimoReal_;
    if (dt > 1.0 / 90.0 && dt < 0.2) intervalo_ = intervalo_ * 0.8 + dt * 0.2;
  }
  ultimoReal_ = ahora;
  reales_++;
  /* 1) el real a la entrada b%2, cuando el host acabo de leerla (el final del lote anterior) */
  const size_t k = cbSig_;
  cbSig_ = (cbSig_ + 1) % cbs_.size();
  VkCommandBuffer &cb = cbs_[k];
  /* con plazo: un host colgado apaga la generacion en vez de colgar el visor */
  if (vkWaitForFences(d, 1, &vallas_[k], VK_TRUE, 1000000000ull) != VK_SUCCESS) {
    if (err) *err = "mako-fg-host no acabo a tiempo";
    return false;
  }
  vkResetFences(d, 1, &vallas_[k]);
  vkResetCommandBuffer(cb, 0);
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);
  const Img &ent = fuente_[(size_t)(lote_ % 2)];
  const uint32_t fam = dev_->queueFamily();
  VkImageMemoryBarrier pre[2] = {barrera(ent.img, VK_QUEUE_FAMILY_EXTERNAL, fam, 0, VK_ACCESS_TRANSFER_WRITE_BIT),
                                 barrera(src.img, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, VK_ACCESS_SHADER_READ_BIT,
                                         VK_ACCESS_TRANSFER_READ_BIT)};
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, pre);
  VkImageCopy c{};
  c.srcSubresource = c.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  c.extent = {(uint32_t)w_, (uint32_t)h_, 1};
  vkCmdCopyImage(cb, src.img, VK_IMAGE_LAYOUT_GENERAL, ent.img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
  VkImageMemoryBarrier post[2] = {barrera(ent.img, fam, VK_QUEUE_FAMILY_EXTERNAL, VK_ACCESS_TRANSFER_WRITE_BIT, 0),
                                  barrera(src.img, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, VK_ACCESS_TRANSFER_READ_BIT,
                                          VK_ACCESS_SHADER_READ_BIT)};
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, post);
  vkEndCommandBuffer(cb);
  const uint64_t v = valor_;
  if (!enviar(k, ultimoFin_, v, err)) return false;
  /* 2) Schedule: el primero solo arranca la historia; los demas piden los N entre el real anterior y este */
  Peticion s{};
  s.op = Schedule, s.interval = (float)intervalo_;
  const bool genera = lote_ > 0;
  if (genera) {
    s.count = n_;
    for (uint32_t i = 0; i < n_; i++) s.times[i] = (float)(i + 1) / (float)(n_ + 1);
  }
  Respuesta r{};
  if (!pedir(ctx_, s, r) || r.failed) {
    if (err) *err = r.failed ? std::string("mako-fg-host: ") + r.text : "mako-fg-host dejo de contestar";
    return false;
  }
  lote_++;
  ultimoFin_ = genera ? v + n_ : v;
  valor_ = ultimoFin_ + 1;
  plan_.clear();
  if (!genera) return true;
  /* 3) los generados al anillo, en cuanto el host los señale; y el plan: uno cada intervalo/(N+1), luego el real */
  const size_t k2 = cbSig_;
  cbSig_ = (cbSig_ + 1) % cbs_.size();
  VkCommandBuffer &cb2 = cbs_[k2];
  if (vkWaitForFences(d, 1, &vallas_[k2], VK_TRUE, 1000000000ull) != VK_SUCCESS) {
    if (err) *err = "mako-fg-host no acabo a tiempo";
    return false;
  }
  vkResetFences(d, 1, &vallas_[k2]);
  vkResetCommandBuffer(cb2, 0);
  vkBeginCommandBuffer(cb2, &bi);
  std::vector<VkImageMemoryBarrier> adq, sue;
  std::vector<size_t> destinos;
  for (uint32_t i = 0; i < n_; i++) {
    const size_t a = anilloSig_;
    anilloSig_ = (anilloSig_ + 1) % anillo_.size();
    destinos.push_back(a);
    adq.push_back(barrera(destino_[i].img, VK_QUEUE_FAMILY_EXTERNAL, fam, 0, VK_ACCESS_TRANSFER_READ_BIT));
    adq.push_back(barrera(anillo_[a].img, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT));
    sue.push_back(barrera(destino_[i].img, fam, VK_QUEUE_FAMILY_EXTERNAL, VK_ACCESS_TRANSFER_READ_BIT, 0));
    sue.push_back(barrera(anillo_[a].img, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT));
  }
  vkCmdPipelineBarrier(cb2, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                       (uint32_t)adq.size(), adq.data());
  for (uint32_t i = 0; i < n_; i++)
    vkCmdCopyImage(cb2, destino_[i].img, VK_IMAGE_LAYOUT_GENERAL, anillo_[destinos[i]].img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
  /* lo que dibuje despues en esta cola (la trama) espera a la copia */
  vkCmdPipelineBarrier(cb2, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
                       (uint32_t)sue.size(), sue.data());
  vkEndCommandBuffer(cb2);
  /* espera al ultimo generado del lote y no señala nada: la cuenta del semaforo la lleva el host (el siguiente real tiene
     que ser v+N+1). El siguiente real va despues en esta misma cola, asi que su señal llega cuando esta copia acabo y el
     host ya puede escribir encima de sus salidas */
  if (!enviar(k2, ultimoFin_, 0, err)) return false;
  const double paso = intervalo_ / (double)(n_ + 1);
  for (uint32_t i = 0; i < n_; i++) plan_.push_back({ahora + paso * (double)i, anillo_[destinos[i]].view});
  plan_.push_back({ahora + paso * (double)n_, VK_NULL_HANDLE});   /* y el real */
  return true;
}

VkImageView FgVideo::vista(double ahora, bool *cambio) {
  VkImageView v = ultimaVista_;
  for (const auto &p : plan_)
    if (p.first <= ahora) v = p.second;
  if (cambio) *cambio = v != ultimaVista_;
  if (v != ultimaVista_ && v != VK_NULL_HANDLE) generadosVistos_++;
  ultimaVista_ = v;
  return v;
}
