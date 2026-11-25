/* vkweave.cpp — intercambio de imágenes, pipelines de trama e interfaz, y el ciclo de cuadro.
   Se apoya en VkDev (instancia, cola, importación de DMA-BUF) y en Hud (geometría en CPU). */
#include "vkweave.h"

#include "hud_frag_sp.h"
#include "hud_vert_sp.h"
#include "weave_frag_sp.h"
#include "weave_vert_sp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hoyos.h"

namespace {
constexpr size_t kMaxHudVerts = 65536;
/* std140: vec2 v2c[4] en 0, vec2 v3c[4] en 64, vec3 v4c[4] en 128 (paso 16, no 12),
   vec4 res en 192, tun1 en 208, tun2 en 224, tun3 en 240. */
/* Bloque std140 de weave.frag: cada arreglo de vec2 o vec3 ocupa 64 bytes (la alineacion de un
   arreglo se redondea a 16) y despues vienen ocho vec4: el quinto es el del conversor 2D/3D, el
   sexto los ajustes finos del mismo (temblor y topes por ojo), el septimo la medicion de la trama
   y el octavo las capas por profundidad (cuanto se estira el fondo y cuanto el frente). */
constexpr size_t kOffV2 = 0, kOffV3 = 64, kOffV4 = 128, kOffRes = 192, kOffTun1 = 208, kOffTun2 = 224,
                 kOffTun3 = 240, kOffTun4 = 256, kOffTun5 = 272, kOffTun6 = 288, kOffTun7 = 304,
                 kOffTun8 = 320;
/* kUboBytes esta declarado en vkweave.h para que el almacen en CPU use el mismo numero. Si se
   anade otro vec4 al sombreador, hay que subir kUboBytes de 16 en 16 Y anadir su kOff... aqui:
   este aviso salta en la compilacion si los desfases se salen del bloque. */
static_assert(kOffTun8 + 16 <= kUboBytes, "el ultimo bloque del UBO se sale de kUboBytes");

/* esquinas del campo en el mismo orden que la ruta OpenGL: TL, TR, BL, BR */
const double kNdc[4][2] = {{-1.0, -1.0}, {1.0, -1.0}, {-1.0, 1.0}, {1.0, 1.0}};

VkShaderModule makeModule(VkDevice dev, const unsigned char *spv, size_t n, std::string *err) {
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = n;
  ci.pCode = (const uint32_t *)spv;
  VkShaderModule m = VK_NULL_HANDLE;
  VkResult r = vkCreateShaderModule(dev, &ci, nullptr, &m);
  if (r != VK_SUCCESS && err) *err = std::string("vkCreateShaderModule: ") + vkResultName(r);
  return m;
}
}  // namespace

/* ------------------------------------------------------------------ arranque ------------------------------------------------------------------ */

bool VkWeave::init(VkDev &dev, VkSurfaceKHR surface, int w, int h, std::string *err) {
  dev_ = &dev;
  surface_ = surface;
  if (!createSwapchain(w, h, err)) return false;

  /* conjunto de la trama: 0 fuente · 1 corrA · 2 corrB · 3 parámetros · 4 profundidad */
  VkDescriptorSetLayoutBinding b[5]{};
  for (int i : {0, 1, 2, 4}) {
    b[i].binding = (uint32_t)i;
    b[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[i].descriptorCount = 1;
    b[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  b[3].binding = 3;
  b[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  b[3].descriptorCount = 1;
  b[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dli.bindingCount = 5;
  dli.pBindings = b;
  if (vkCreateDescriptorSetLayout(dev.device(), &dli, nullptr, &srcSetLay_) != VK_SUCCESS) {
    if (err) *err = "vkCreateDescriptorSetLayout de la trama falló";
    return false;
  }
  VkDescriptorSetLayoutBinding hb{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo hdli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  hdli.bindingCount = 1;
  hdli.pBindings = &hb;
  if (vkCreateDescriptorSetLayout(dev.device(), &hdli, nullptr, &hudSetLay_) != VK_SUCCESS) {
    if (err) *err = "vkCreateDescriptorSetLayout de la interfaz falló";
    return false;
  }

  /* un par de conjuntos por hueco del intercambio, mas los dos que pide un cuadro fuera de pantalla */
  VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7 * (uint32_t)imgs_.size() + 10},
                                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 * (uint32_t)imgs_.size() + 4}};
  VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pci.maxSets = 4 * (uint32_t)imgs_.size() + 8;
  pci.poolSizeCount = 2;
  pci.pPoolSizes = ps;
  if (vkCreateDescriptorPool(dev.device(), &pci, nullptr, &pool_) != VK_SUCCESS) {
    if (err) *err = "vkCreateDescriptorPool falló";
    return false;
  }

  if (!createPipelines(err)) return false;

  /* reserva negra y corrección neutra (A = 0, B = medio gris) hasta que lleguen los mapas */
  std::string e2;
  static const uint8_t negro[4] = {0, 0, 0, 255};
  static const uint8_t cero[1] = {0}, medio[1] = {128}, medio2[2] = {128, 128};
  uploadRgba8(negro, 1, 1, black_, &e2);
  uploadR8(cero, 1, 1, corrA_, &e2);
  uploadR8(medio, 1, 1, corrB_, &e2);
  /* el shader siempre necesita una textura valida, y la suya lleva un canal por ojo */
  uploadR8(medio2, 1, 1, depth_, &e2, VK_FORMAT_R8G8_UNORM, 2);
  return createFrames(err);
}

bool VkWeave::createSwapchain(int w, int h, std::string *err) {
  VkSurfaceCapabilitiesKHR caps{};
  VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev_->phys(), surface_, &caps);
  if (r != VK_SUCCESS) {
    if (err) *err = std::string("capacidades de la superficie: ") + vkResultName(r);
    return false;
  }
  uint32_t nf = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(dev_->phys(), surface_, &nf, nullptr);
  std::vector<VkSurfaceFormatKHR> fmts(nf);
  vkGetPhysicalDeviceSurfaceFormatsKHR(dev_->phys(), surface_, &nf, fmts.data());
  bool picked = false;
  for (const VkSurfaceFormatKHR &sf : fmts) {
    if ((sf.format == VK_FORMAT_B8G8R8A8_UNORM || sf.format == VK_FORMAT_R8G8B8A8_UNORM) &&
        sf.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      fmt_ = sf.format, cs_ = sf.colorSpace;
      picked = true;
      break;
    }
  }
  if (!picked && nf) fmt_ = fmts[0].format, cs_ = fmts[0].colorSpace;
  if (fmt_ == VK_FORMAT_UNDEFINED) {
    if (err) *err = "la superficie no ofrece formato de color";
    return false;
  }

  uint32_t np = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(dev_->phys(), surface_, &np, nullptr);
  std::vector<VkPresentModeKHR> modes(np);
  vkGetPhysicalDeviceSurfacePresentModesKHR(dev_->phys(), surface_, &np, modes.data());
  pm_ = VK_PRESENT_MODE_FIFO_KHR; /* al ritmo del panel: es lo que necesita la trama */
  bool haveFifo = false;
  for (VkPresentModeKHR m : modes)
    if (m == VK_PRESENT_MODE_FIFO_KHR) haveFifo = true;
  if (!haveFifo && np) pm_ = modes[0];

  ext_ = caps.currentExtent;
  if (ext_.width == 0xFFFFFFFFu) {
    ext_.width = std::min<std::uint32_t>((uint32_t)w, caps.maxImageExtent.width);
    ext_.height = std::min<std::uint32_t>((uint32_t)h, caps.maxImageExtent.height);
    ext_.width = std::max(ext_.width, caps.minImageExtent.width);
    ext_.height = std::max(ext_.height, caps.minImageExtent.height);
  }
  uint32_t nImg = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && nImg > caps.maxImageCount) nImg = caps.maxImageCount;

  VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  sci.surface = surface_;
  sci.minImageCount = nImg;
  sci.imageFormat = fmt_;
  sci.imageColorSpace = cs_;
  sci.imageExtent = ext_;
  sci.imageArrayLayers = 1;
  sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  sci.preTransform = caps.currentTransform;
  if (!(caps.supportedTransforms & caps.currentTransform)) sci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  /* KWin no acepta INHERIT: elegimos el primero que la superficie soporte. */
  VkSurfaceCapabilitiesKHR cap{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev_->phys(), surface_, &cap);
  VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  for (VkCompositeAlphaFlagBitsKHR bit : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                          VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    if (cap.supportedCompositeAlpha & bit) { alpha = bit; break; }
  sci.compositeAlpha = alpha;
  sci.presentMode = pm_;
  sci.clipped = VK_TRUE;
  sci.oldSwapchain = VK_NULL_HANDLE;
  r = vkCreateSwapchainKHR(dev_->device(), &sci, nullptr, &sc_);
  if (r != VK_SUCCESS) {
    if (err) *err = std::string("vkCreateSwapchainKHR: ") + vkResultName(r);
    return false;
  }
  uint32_t n = 0;
  vkGetSwapchainImagesKHR(dev_->device(), sc_, &n, nullptr);
  imgs_.assign(n, VK_NULL_HANDLE);
  vkGetSwapchainImagesKHR(dev_->device(), sc_, &n, imgs_.data());
  views_.assign(n, VK_NULL_HANDLE);
  for (uint32_t i = 0; i < n; i++) {
    VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    iv.image = imgs_[i];
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format = fmt_;
    iv.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(dev_->device(), &iv, nullptr, &views_[i]) != VK_SUCCESS) {
      if (err) *err = "vkCreateImageView del intercambio falló";
      return false;
    }
  }
  std::printf("[vulkan] intercambio %ux%u · %u imágenes · %s · %s\n", ext_.width, ext_.height, n,
              VkDev::vkFormatName(fmt_), pm_ == VK_PRESENT_MODE_FIFO_KHR ? "fifo" : "otro modo");
  return true;
}

void VkWeave::destroySwapchain() {
  if (!dev_) return;
  for (VkImageView v : views_)
    if (v) vkDestroyImageView(dev_->device(), v, nullptr);
  views_.clear();
  imgs_.clear();
  if (sc_) vkDestroySwapchainKHR(dev_->device(), sc_, nullptr);
  sc_ = VK_NULL_HANDLE;
}

bool VkWeave::createPipelines(std::string *err) {
  VkPushConstantRange pc{VK_SHADER_STAGE_VERTEX_BIT, 0, 8};
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pli.setLayoutCount = 1;
  pli.pSetLayouts = &srcSetLay_;
  if (vkCreatePipelineLayout(dev_->device(), &pli, nullptr, &weaveLay_) != VK_SUCCESS) {
    if (err) *err = "vkCreatePipelineLayout de la trama falló";
    return false;
  }
  pli.pPushConstantRanges = &pc;
  pli.pushConstantRangeCount = 1;
  pli.pSetLayouts = &hudSetLay_;
  if (vkCreatePipelineLayout(dev_->device(), &pli, nullptr, &hudLay_) != VK_SUCCESS) {
    if (err) *err = "vkCreatePipelineLayout de la interfaz falló";
    return false;
  }

  VkPipelineRenderingCreateInfo ri{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  ri.colorAttachmentCount = 1;
  ri.pColorAttachmentFormats = &fmt_;

  struct Spec {
    const unsigned char *vert, *frag;
    size_t vlen, flen;
    VkPipelineLayout lay;
    VkPipeline *out;
    bool attribs, blend;
  };
  const Spec specs[2] = {{weaveVertSpv, weaveFragSpv, weaveVertSpv_len, weaveFragSpv_len, weaveLay_, &weavePipe_, false,
                          false},
                         {hudVertSpv, hudFragSpv, hudVertSpv_len, hudFragSpv_len, hudLay_, &hudPipe_, true, true}};
  for (const Spec &sp : specs) {
    VkShaderModule vs = makeModule(dev_->device(), sp.vert, sp.vlen, err);
    VkShaderModule fs = makeModule(dev_->device(), sp.frag, sp.flen, err);
    if (!vs || !fs) return false;
    VkGraphicsPipelineCreateInfo gpi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpi.pNext = &ri;
    VkPipelineShaderStageCreateInfo st[2]{};
    st[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = vs;
    st[0].pName = "main";
    st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = fs;
    st[1].pName = "main";
    gpi.stageCount = 2;
    gpi.pStages = st;

    /* la trama no tiene vértices en memoria; la interfaz usa posición, uv y color */
    VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkVertexInputBindingDescription bind{0, (uint32_t)sizeof(HudVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attr[3] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
                                                 {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
                                                 {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16}};
    if (sp.attribs) {
      vin.vertexBindingDescriptionCount = 1;
      vin.pVertexBindingDescriptions = &bind;
      vin.vertexAttributeDescriptionCount = 3;
      vin.pVertexAttributeDescriptions = attr;
    }
    gpi.pVertexInputState = &vin;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    gpi.pInputAssemblyState = &ia;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    gpi.pViewportState = &vp;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;
    gpi.pRasterizationState = &rs;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    gpi.pMultisampleState = &ms;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    gpi.pDepthStencilState = &ds;
    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = sp.blend ? VK_TRUE : VK_FALSE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    gpi.pColorBlendState = &cb;
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dsi.dynamicStateCount = 2;
    dsi.pDynamicStates = dyn;
    gpi.pDynamicState = &dsi;
    gpi.layout = sp.lay;
    VkResult r2 = vkCreateGraphicsPipelines(dev_->device(), VK_NULL_HANDLE, 1, &gpi, nullptr, sp.out);
    vkDestroyShaderModule(dev_->device(), vs, nullptr);
    vkDestroyShaderModule(dev_->device(), fs, nullptr);
    if (r2 != VK_SUCCESS) {
      if (err) *err = std::string("vkCreateGraphicsPipelines: ") + vkResultName(r2);
      return false;
    }
  }
  return true;
}

void VkWeave::writeWeaveSet(VkDescriptorSet set, VkImageView srcView, VkBuffer ubo) {
  const VkSampler samp = dev_->sampler();
  VkDescriptorImageInfo si[4] = {{samp, srcView, VK_IMAGE_LAYOUT_GENERAL},
                                 {samp, corrA_.view, VK_IMAGE_LAYOUT_GENERAL},
                                 {samp, corrB_.view, VK_IMAGE_LAYOUT_GENERAL},
                                 {samp, depth_.view, VK_IMAGE_LAYOUT_GENERAL}};
  VkDescriptorBufferInfo bi{ubo, 0, kUboBytes};
  VkWriteDescriptorSet wr[5]{};
  for (int i = 0; i < 3; i++) {
    wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[i].dstSet = set;
    wr[i].dstBinding = (uint32_t)i;
    wr[i].descriptorCount = 1;
    wr[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr[i].pImageInfo = &si[i];
  }
  wr[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  wr[3].dstSet = set;
  wr[3].dstBinding = 3;
  wr[3].descriptorCount = 1;
  wr[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  wr[3].pBufferInfo = &bi;
  wr[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  wr[4].dstSet = set;
  wr[4].dstBinding = 4;
  wr[4].descriptorCount = 1;
  wr[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  wr[4].pImageInfo = &si[3];
  vkUpdateDescriptorSets(dev_->device(), 5, wr, 0, nullptr);
}

void VkWeave::writeHudSet(VkDescriptorSet set, VkImageView atlasView) {
  VkDescriptorImageInfo ai{dev_->sampler(), atlasView, VK_IMAGE_LAYOUT_GENERAL};
  VkWriteDescriptorSet hw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  hw.dstSet = set;
  hw.dstBinding = 0;
  hw.descriptorCount = 1;
  hw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  hw.pImageInfo = &ai;
  vkUpdateDescriptorSets(dev_->device(), 1, &hw, 0, nullptr);
}

VkDescriptorSet VkWeave::newSet(VkDescriptorSetLayout lay) {
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  ai.descriptorPool = pool_;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &lay;
  VkDescriptorSet s = VK_NULL_HANDLE;
  if (vkAllocateDescriptorSets(dev_->device(), &ai, &s) != VK_SUCCESS) return VK_NULL_HANDLE;
  return s;
}

bool VkWeave::createFrames(std::string *err) {
  if (cmdPool_ == VK_NULL_HANDLE) {
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = dev_->queueFamily();
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(dev_->device(), &cpi, nullptr, &cmdPool_) != VK_SUCCESS) {
      if (err) *err = "vkCreateCommandPool falló";
      return false;
    }
  }
  slots_.assign(imgs_.size(), FrameSlot{});
  for (size_t i = 0; i < slots_.size(); i++) {
    FrameSlot &s = slots_[i];
    std::string e2;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmdPool_;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(dev_->device(), &cai, &s.cmd) != VK_SUCCESS) {
      if (err) *err = "vkAllocateCommandBuffers falló";
      return false;
    }
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(dev_->device(), &fci, nullptr, &s.fence) != VK_SUCCESS) {
      if (err) *err = "vkCreateFence falló";
      return false;
    }
    VkSemaphoreCreateInfo smi{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(dev_->device(), &smi, nullptr, &s.sem) != VK_SUCCESS) {
      if (err) *err = "vkCreateSemaphore falló";
      return false;
    }
    s.weave = newSet(srcSetLay_);
    s.hud = newSet(hudSetLay_);
    if (!s.weave || !s.hud) {
      if (err) *err = "el charco de descriptores se quedó vacío";
      return false;
    }
    if (!dev_->createBuffer(kUboBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, s.ubo,
                            s.uboMem, &e2)) {
      if (err) *err = "buffer de parámetros: " + e2;
      return false;
    }
    if (!dev_->createBuffer(kMaxHudVerts * sizeof(HudVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, s.vbo,
                            s.vboMem, &e2)) {
      if (err) *err = "buffer de la interfaz: " + e2;
      return false;
    }
    s.vboSize = kMaxHudVerts * sizeof(HudVertex);
  }
  return true;
}

void VkWeave::destroyFrames() {
  if (!dev_) return;
  for (FrameSlot &s : slots_) {
    if (s.cmd && cmdPool_) vkFreeCommandBuffers(dev_->device(), cmdPool_, 1, &s.cmd);
    if (s.fence) vkDestroyFence(dev_->device(), s.fence, nullptr);
    if (s.sem) vkDestroySemaphore(dev_->device(), s.sem, nullptr);
    if (s.weave && pool_) vkFreeDescriptorSets(dev_->device(), pool_, 1, &s.weave);
    if (s.hud && pool_) vkFreeDescriptorSets(dev_->device(), pool_, 1, &s.hud);
    if (s.ubo) vkDestroyBuffer(dev_->device(), s.ubo, nullptr);
    if (s.uboMem) vkFreeMemory(dev_->device(), s.uboMem, nullptr);
    if (s.vbo) vkDestroyBuffer(dev_->device(), s.vbo, nullptr);
    if (s.vboMem) vkFreeMemory(dev_->device(), s.vboMem, nullptr);
    s = FrameSlot{};
  }
  slots_.clear();
  if (cmdPool_) {
    vkDestroyCommandPool(dev_->device(), cmdPool_, nullptr);
    cmdPool_ = VK_NULL_HANDLE;
  }
}

void VkWeave::waitIdle() {
  if (dev_ && dev_->device()) vkDeviceWaitIdle(dev_->device());
}

void VkWeave::destroy() {
  if (!dev_) return;
  waitIdle();
  destroyFrames();
  for (Keep &k : keep_) dev_->destroyImg(k.img);
  keep_.clear();
  dev_->destroyImg(next_);
  next_ = VkImg{};
  dev_->destroyImg(black_);
  dev_->destroyImg(corrA_);
  dev_->destroyImg(corrB_);
  dev_->destroyImg(atlas_);
  dev_->destroyImg(small_);
  dev_->destroyImg(depth_);
  if (smallStage_) vkDestroyBuffer(dev_->device(), smallStage_, nullptr);
  if (smallStageMem_) vkFreeMemory(dev_->device(), smallStageMem_, nullptr);
  if (mapStage_) vkDestroyBuffer(dev_->device(), mapStage_, nullptr);
  if (mapStageMem_) vkFreeMemory(dev_->device(), mapStageMem_, nullptr);
  mapStage_ = VK_NULL_HANDLE;
  mapStageMem_ = VK_NULL_HANDLE;
  depthW_ = depthH_ = 0;
  if (smallUbo_) vkDestroyBuffer(dev_->device(), smallUbo_, nullptr);
  if (smallUboMem_) vkFreeMemory(dev_->device(), smallUboMem_, nullptr);
  smallStage_ = smallUbo_ = VK_NULL_HANDLE;
  smallStageMem_ = smallUboMem_ = VK_NULL_HANDLE;
  smallSet_ = VK_NULL_HANDLE;
  smallW_ = smallH_ = 0;
  depthUsable_ = false;
  if (weavePipe_) vkDestroyPipeline(dev_->device(), weavePipe_, nullptr);
  if (hudPipe_) vkDestroyPipeline(dev_->device(), hudPipe_, nullptr);
  if (weaveLay_) vkDestroyPipelineLayout(dev_->device(), weaveLay_, nullptr);
  if (hudLay_) vkDestroyPipelineLayout(dev_->device(), hudLay_, nullptr);
  if (srcSetLay_) vkDestroyDescriptorSetLayout(dev_->device(), srcSetLay_, nullptr);
  if (hudSetLay_) vkDestroyDescriptorSetLayout(dev_->device(), hudSetLay_, nullptr);
  if (pool_) vkDestroyDescriptorPool(dev_->device(), pool_, nullptr);
  pool_ = VK_NULL_HANDLE;
  destroySwapchain();
  dev_ = nullptr;
}

bool VkWeave::resize(int w, int h) {
  if (!dev_) return false;
  waitIdle();
  destroyFrames();
  for (Keep &k : keep_) dev_->destroyImg(k.img);
  keep_.clear();
  dev_->destroyImg(next_);
  next_ = VkImg{};
  destroySwapchain();
  if (weavePipe_) vkDestroyPipeline(dev_->device(), weavePipe_, nullptr), weavePipe_ = VK_NULL_HANDLE;
  if (hudPipe_) vkDestroyPipeline(dev_->device(), hudPipe_, nullptr), hudPipe_ = VK_NULL_HANDLE;
  std::string err;
  if (!createSwapchain(w, h, &err) || !createPipelines(&err) || !createFrames(&err)) {
    std::fprintf(stderr, "[vulkan] no se pudo rehacer el dibujado: %s\n", err.c_str());
    return false;
  }
  lost_ = false;
  return true;
}

/* ------------------------------------------------------------------ imágenes ------------------------------------------------------------------ */

void VkWeave::barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to, VkAccessFlags srcA,
                      VkAccessFlags dstA) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask = srcA;
  b.dstAccessMask = dstA;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = img;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &b);
}

bool VkWeave::uploadR8(const uint8_t *px, int w, int h, VkImg &out, std::string *err, VkFormat fmt, int canales) {
  if (!px || w <= 0 || h <= 0 || canales < 1) {
    if (err) *err = "imagen de 8 bits sin datos";
    return false;
  }
  std::string e2;
  dev_->destroyImg(out);
  const VkDeviceSize n = (VkDeviceSize)w * (VkDeviceSize)h * (VkDeviceSize)canales;
  if (!dev_->createImage(w, h, fmt, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, out,
                         &e2)) {
    if (err) *err = "imagen R8: " + e2;
    return false;
  }
  VkBuffer stage = VK_NULL_HANDLE;
  VkDeviceMemory stageMem = VK_NULL_HANDLE;
  bool ok = dev_->createBuffer(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stage,
                               stageMem, &e2);
  if (ok) ok = dev_->uploadBytes(px, n, stage, stageMem, &e2);
  if (!ok) {
    if (err) *err = "buffer de paso: " + e2;
    if (stage) vkDestroyBuffer(dev_->device(), stage, nullptr);
    if (stageMem) vkFreeMemory(dev_->device(), stageMem, nullptr);
    return false;
  }
  VkCommandBuffer cb = dev_->beginOneTime(&e2);
  if (!cb) {
    if (err) *err = "buffer de mando: " + e2;
    vkDestroyBuffer(dev_->device(), stage, nullptr);
    vkFreeMemory(dev_->device(), stageMem, nullptr);
    return false;
  }
  barrier(cb, out.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
          VK_ACCESS_TRANSFER_WRITE_BIT);
  VkBufferImageCopy reg{};
  reg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  reg.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
  vkCmdCopyBufferToImage(cb, stage, out.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
  barrier(cb, out.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_SHADER_READ_BIT);
  ok = dev_->endOneTime(cb, &e2);
  vkDestroyBuffer(dev_->device(), stage, nullptr);
  vkFreeMemory(dev_->device(), stageMem, nullptr);
  if (!ok && err) *err = e2;
  return ok;
}

bool VkWeave::uploadRgba8(const uint8_t *px, int w, int h, VkImg &out, std::string *err) {
  if (!px || w <= 0 || h <= 0) {
    if (err) *err = "imagen RGBA sin datos";
    return false;
  }
  std::string e2;
  dev_->destroyImg(out);
  VkDeviceSize n = (VkDeviceSize)w * (VkDeviceSize)h * 4u;
  if (!dev_->createImage(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         out, &e2)) {
    if (err) *err = "imagen RGBA: " + e2;
    return false;
  }
  VkBuffer stage = VK_NULL_HANDLE;
  VkDeviceMemory stageMem = VK_NULL_HANDLE;
  bool ok = dev_->createBuffer(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stage,
                               stageMem, &e2);
  if (ok) ok = dev_->uploadBytes(px, n, stage, stageMem, &e2);
  if (!ok) {
    if (err) *err = "buffer de paso: " + e2;
    if (stage) vkDestroyBuffer(dev_->device(), stage, nullptr);
    if (stageMem) vkFreeMemory(dev_->device(), stageMem, nullptr);
    return false;
  }
  VkCommandBuffer cb = dev_->beginOneTime(&e2);
  if (!cb) {
    if (err) *err = "buffer de mando: " + e2;
    vkDestroyBuffer(dev_->device(), stage, nullptr);
    vkFreeMemory(dev_->device(), stageMem, nullptr);
    return false;
  }
  barrier(cb, out.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
  VkBufferImageCopy reg{};
  reg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  reg.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
  vkCmdCopyBufferToImage(cb, stage, out.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
  barrier(cb, out.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_SHADER_READ_BIT);
  ok = dev_->endOneTime(cb, &e2);
  vkDestroyBuffer(dev_->device(), stage, nullptr);
  vkFreeMemory(dev_->device(), stageMem, nullptr);
  if (!ok && err) *err = e2;
  return ok;
}

bool VkWeave::sourceFromDma(const GpuFrame &f, std::string *err) {
  VkImg img{};
  if (!dev_->importImage(f, img, err, false) && !dev_->importImageImplicit(f, img, err, err)) return false;
  dev_->destroyImg(next_);
  next_ = img;
  return true;
}

bool VkWeave::sourceFromRgba(const uint8_t *rgba, int w, int h, bool topFirst, std::string *err) {
  std::vector<uint8_t> buf;
  const uint8_t *src = rgba;
  if (!topFirst) {
    buf.assign((size_t)w * (size_t)h * 4u, 0);
    for (int y = 0; y < h; y++)
      memcpy(&buf[(size_t)y * (size_t)w * 4u], rgba + (size_t)(h - 1 - y) * (size_t)w * 4u, (size_t)w * 4u);
    src = buf.data();
  }
  VkImg img{};
  if (!uploadRgba8(src, w, h, img, err)) return false;
  dev_->destroyImg(next_);
  next_ = img;
  return true;
}

void VkWeave::setCorrections(const uint8_t *a, int aw, int ah, const uint8_t *b, int bw, int bh) {
  if (!dev_ || (!a && !b)) return;
  waitIdle();
  std::string err;
  if (a && !uploadR8(a, aw, ah, corrA_, &err)) std::fprintf(stderr, "[trama vk] mapa A: %s\n", err.c_str());
  if (b && !uploadR8(b, bw, bh, corrB_, &err)) std::fprintf(stderr, "[trama vk] mapa B: %s\n", err.c_str());
}

void VkWeave::setAtlas(const std::vector<uint8_t> &px, int w, int h) {
  atlasPx_ = px;
  atlasW_ = w;
  atlasH_ = h;
  atlasDirty_ = true;
}

/* ------------------------------------------------------------------ cuadro ------------------------------------------------------------------ */

void VkWeave::fillUbo(uint8_t *dst) {
  memset(dst, 0, kUboBytes);
  CornerAttrs fa[4];
  float mn = 1e30f;
  for (int i = 0; i < 4; i++) {
    fa[i] = lensCorner(field_, eye_, pw_, ph_, kNdc[i][0], kNdc[i][1]);
    for (int c = 0; c < 3; c++) mn = mn < fa[i].v4[c] ? mn : fa[i].v4[c];
  }
  int v4min = (int)std::floor((double)mn);
  float *v2 = (float *)(dst + kOffV2), *v3 = (float *)(dst + kOffV3), *v4 = (float *)(dst + kOffV4);
  float *res = (float *)(dst + kOffRes), *t1 = (float *)(dst + kOffTun1), *t2 = (float *)(dst + kOffTun2),
        *t3 = (float *)(dst + kOffTun3);
  for (int i = 0; i < 4; i++) {
    v2[i * 2 + 0] = fa[i].v2[0];
    v2[i * 2 + 1] = fa[i].v2[1];
    v3[i * 2 + 0] = fa[i].v3[0];
    v3[i * 2 + 1] = fa[i].v3[1];
    /* vec3 en std140 ocupa 16 bytes: el cuarto hueco queda sin usar */
    for (int c = 0; c < 3; c++) v4[i * 4 + c] = fa[i].v4[c] - (float)v4min;
  }
  res[0] = (float)ext_.width;
  res[1] = (float)ext_.height;
  res[2] = tune_.filterSlope;
  res[3] = tune_.xtalkFac;
  t1[0] = tune_.xtalkDyn;
  t1[1] = tune_.contrast;
  t1[2] = tune_.corrAScale;
  t1[3] = tune_.conv;
  t2[0] = tune_.align;
  t2[1] = tune_.pitch;
  t2[2] = tune_.weave ? 1.0f : 0.0f;
  t2[3] = tune_.swapLR ? 1.0f : 0.0f;
  t3[0] = (float)hudState_;
  t3[1] = tune_.topBottom ? 1.0f : 0.0f;
  t3[2] = srcFlipY_ ? 1.0f : 0.0f;
  t3[3] = depthShift_;
  float *t4 = (float *)(dst + kOffTun4);
  t4[0] = (convOn_ && depthUsable_) ? 1.0f : 0.0f;   /* conversor activo de verdad */
  t4[1] = convGain_;                                 /* cuanto desplaza cada ojo */
  t4[2] = convRef_;                                  /* plano que se queda quieto */
  /* conversor puesto pero sin mapa todavia (el primer cuadro, o si la red falla): el shader tiene
     que pintar la imagen ENTERA y plana. Si cae en la rama de lado a lado se ven dos mitades de la
     pantalla una al lado de la otra, que es lo primero que se notaba al encender el conversor. */
  t4[3] = (convOn_ && !depthUsable_) ? 2.0f : 0.0f;
  /* conversor fino: lo que el programa de Samsung hace despues del mapa y aqui faltaba.
     ODYSSEYVK_DITHER  temblor del desplazamiento en pixeles (por defecto 0,35; 0 lo apaga).
                       El de ellos es 0,25 px (dither_strength 0,01 sobre 25 px de recorrido).
     ODYSSEYVK_TOPE    por defecto limita cada ojo a lo que limita el de ellos: +12,67 px
                       hacia afuera y -9,98 px hacia adentro (fraccion del ancho, leido del config
                       oficial: virt.max_disparity 0,0033 / min_disparity -0,0026). Con
                       ODYSSEYVK_TOPE=0 se quita el tope. */
  float *t5 = (float *)(dst + kOffTun5);
  /* Desde v0.51 estos dos numeros son miembros del visor, no constantes leidas una vez: la ventana
     de desarrollo (`odysseyvk-ajustes`, o `ctl ajuste`) los mueve con la trama en marcha. */
  /* ODYSSEYVK_CANTO  ancho del puente del canto (0,004 de fabrica). Cuanto mas chico, menos se le
     quita desplazamiento a un canto donde el mapa cambia mucho: con el relleno de huecos el mapa
     pone mas filo, asi que este numero es el que manda cuanto relieve conserva el borde de un
     objeto. 0 en el shader significa "el de fabrica", por eso se manda siempre positivo. */
  t5[0] = temblorPx_;
  t5[1] = conTope_ ? 12.67f : 0.0f;
  t5[2] = conTope_ ? 9.98f : 0.0f;
  t5[3] = cantoS_;
  /* ODYSSEYVK_FASE_TEMBLOR  temblor de la propia fase de la trama, en ciclos de la lente.
     Por defecto 0 (como venia). Con 0,1 la franja de 4,3 px de la trama baja unas cinco veces
     (medido en tools/mide_franjas.py sobre la propia formula del shader) y el 3D casi no se
     mueve, porque la fundamental pierde solo un 7%. Sirve para ver si las franjas finas que
     quedan con el releve apagado salen del corte brusco del filtro. */
  float *t6 = (float *)(dst + kOffTun6);
  t6[0] = faseTemblor_;
  /* ODYSSEYVK_RAMPA_X y ODYSSEYVK_RAMPA_Y  rampa limpia de fase en ciclos por pixel de panel,
     la misma para los tres canales. Mueve el paso de la rejilla escrita sin tocar nada mas.
     Medido sobre el cuadro escrito (tools/paso_rejilla.py, `weave --solid`, cuadro solido
     3840x2160; el blanco es lo que escribe el conversor de Windows, kx 0,4501 = 2,2217 px =
     0,34530 mm):
       rampa 0         kx 0,4473  ky -0,1598  2,2358 px = 0,34744 mm  batido a los 353 px = 54,9 mm
       rampa -0,00283  kx 0,4501  ky -0,1598  2,2219 px = 0,34529 mm  batido a los 3492 mm
     "--pitch" (paso) tambien cuadra el paso (0,995 -> kx 0,4500) pero escala la rampa de la lente
     entera, y ahi van metidos los desfases entre canales rojo/verde/azul: afloja la inclinacion
     (ky -0,1589 en vez de -0,1598). La rampa deja la inclinacion exacta, asi que esa es la llave.
     Tope +/-0,05 ciclos por pixel: mas de ahi ya no es un ajuste fino, es otra rejilla. */
  /* Las teclas `[` `]` (x) y `;` `'` (y) suman rampaDX/rampaDY encima de lo que dice la
     configuracion, para poder afinar el paso mirando la pantalla sin reiniciar nada. El total
     sigue topado a +/-0,05. */
  const auto topaRampa = [](float v) { return (v > -0.05f && v < 0.05f) ? v : (v >= 0.05f ? 0.05f : -0.05f); };
  const float rrX = topaRampa(rampaBaseX_ + tune_.rampaDX);
  const float rrY = topaRampa(rampaBaseY_ + tune_.rampaDY);
  t6[1] = rrX;
  t6[2] = rrY;
  /* Sonda de taller (ODYSSEYVK_PRUEBA): hueco libre del bloque, 0 = normal. El sombreador con
     1 saca la muestra del ojo izquierdo, con 3 la misma textura pero con la coordenada del pase
     2D, con 4 el peso del filtro y con 6 la fase base. Sirve para ver en que punto del calculo
     una imagen con contenido se vuelve una lama de un solo color. */
  static const float kPruebaSombra = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_PRUEBA");
    return e ? (float)std::atof(e) : 0.0f;
  }();
  /* El patron de un solo ojo (tecla `b`, v0.59) va por el mismo hueco con los codigos 12 y 13;
     la sonda de taller, si se pidio por el entorno, manda sobre el. */
  t6[3] = kPruebaSombra != 0.0f ? kPruebaSombra
        : (tune_.unOjo == 1 ? 12.0f : (tune_.unOjo == 2 ? 13.0f : 0.0f));
  /* Capas por profundidad. `tun7.x` cuanto se estira lo que queda DETRAS del plano de apoyo y
     `tun7.y` lo que sale DELANTE; 0 en el hueco = sin escalar (lo de siempre). El visor arranca
     estos valores de ODYSSEYVK_GAN_FONDO / ODYSSEYVK_GAN_FRENTE (que vienen de `fondo` y
     `frente` en serve.conf) y las teclas `j`/`k` y `u`/`i` los mueven en marcha, de 0,05 en 0,05.
     El escalado va antes de los topes por ojo (+12,67 fuera / 9,98 dentro), asi que por mucho que
     se suba el fondo nunca se sale del presupuesto de Samsung. */
  static const float ganBaseFondo = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_GAN_FONDO");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return (v >= 0.2f && v <= 2.0f) ? v : 0.0f;
  }();
  static const float ganBaseFrente = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_GAN_FRENTE");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return (v >= 0.2f && v <= 2.0f) ? v : 0.0f;
  }();
  static const float empBaseFondo = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_EMP_FONDO");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return (v >= -15.0f && v <= 15.0f) ? v : 0.0f;
  }();
  static const float empBaseFrente = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_EMP_FRENTE");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return (v >= -15.0f && v <= 15.0f) ? v : 0.0f;
  }();
  float *t7 = (float *)(dst + kOffTun7);
  /* En cuanto se pulsa una tecla de capas manda lo del visor; mientras no se haya tocado ninguna,
     manda lo de serve.conf. `capaTocada` evita la trampa de volver a 0 con la tecla: sin ella, un
     empuje bajado a 0 caeria de nuevo sobre el valor de arranque. */
  const bool capa = tune_.capaTocada;
  t7[0] = (capa || tune_.ganFondo > 0.0f) ? tune_.ganFondo : ganBaseFondo;
  t7[1] = (capa || tune_.ganFrente > 0.0f) ? tune_.ganFrente : ganBaseFrente;
  t7[2] = capa ? tune_.empFondo : empBaseFondo;
  t7[3] = capa ? tune_.empFrente : empBaseFrente;
  /* Agrandado guiado del mapa (v0.62, ver mapaGuiado en weave.frag). ODYSSEYVK_JBU=0 lo apaga y
     vuelve la cubica con el recorte de canto de siempre; ODYSSEYVK_JBU_COLOR cambia cuanto tiene que
     parecerse el color de un punto del mapa al del pixel para que cuente (0,06 en RGB 0..1). */
  static const bool kJbu = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_JBU");
    return !e || e[0] != '0';
  }();
  static const float kJbuColor = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_JBU_COLOR");
    const float v = e ? (float)std::atof(e) : 0.06f;
    return (v > 0.005f && v < 1.0f) ? v : 0.06f;
  }();
  float *t8 = (float *)(dst + kOffTun8);
  t8[0] = kJbu ? 1.0f : 0.0f;
  t8[1] = kJbuColor;
  t8[2] = 0.8f;   /* ancho del peso por distancia, en puntos del mapa */
  /* Compresion suave en los topes (v0.64, compSuave en weave.frag). ODYSSEYVK_COMPRESION=0 vuelve al
     clamp de siempre. */
  static const bool kCompresion = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_COMPRESION");
    return !e || e[0] != '0';
  }();
  t8[3] = kCompresion ? 1.0f : 0.0f;
  /* Red de seguridad contra NaN y infinito. Un solo numero que no sea un numero dentro de los
     parametros vuelve loco al sombreador: `clamp`, `mix` y hasta la coordenada de una textura con
     NaN no estan definidos, y en la practica el resultado es una pantalla GRIS pareja (el neutro
     del muestreador) en lugar de imagen, que es justo el "veo el visor gris, nada de imagen".
     Aqui no deberia producirse ninguno (el campo de la lente, las teclas y la red entregan
     numeros finitos), pero si alguna vez se cuela, lo cambiamos por el neutro y avisamos: sin esta
     red el fallo es mudo y despues no hay forma de saber de donde vino. */
  {
    static const char *kMiembro[] = {"campo v2", "campo v3", "rampa v4", "res",      "tun1",
                                     "tun2",      "tun3",       "tun4",       "tun5", "tun6",
                                     "tun7",      "tun8"};   /* tun8: si falta, un NaN ahi leia fuera */
    const float *fl = (float *)dst;
    int malos = 0, primero = -1;
    for (int i = 0; i < kUboBytes / 4; i++)
      if (!std::isfinite((double)fl[i])) {
        if (primero < 0) primero = i;
        *(float *)(dst + i * 4) = 0.0f;
        malos++;
      }
    static int avisos = 0;
    if (malos && avisos < 4) {
      avisos++;
      const int off = primero * 4;
      const char *cual = kMiembro[off < 192 ? off / 64 : (off < 208 ? 3 : 4 + (off - 208) / 16)];
      std::fprintf(stderr,
                   "[trama vk] %d parametros de la trama no eran un numero (el primero en %s, "
                   "offset %d): se pusieron en 0 · cuadro %dx%d · conversor %.1f %.1f\n",
                   malos, cual, off, pw_, ph_, t4[0], t4[3]);
    }
  }
  if (std::getenv("ODYSSEY_UBO")) {
    std::fprintf(stderr, "[ubo %p] res %.0fx%.0f slope %.6f xt %.6f xtD %.6f cons %.4f · paso %.6f tramado %.1f · ind %.1f · rampa %.5f %.5f · capas %.2f %.2f · empuje %+.1f %+.1f · v4 TL %.2f %.2f %.2f · conv %.4f plano %.4f dep %.4f · v2 %.4f %.4f corrA %.4f\n",
                 this, res[0], res[1], res[2], res[3], t1[0], t1[1], t2[1], t2[2], t3[0], rrX, rrY, t7[0], t7[1], t7[2], t7[3], v4[0], v4[1], v4[2],
                 t1[3], t4[2], t3[3], v2[0], v2[1], t1[2]);
  }
}

bool VkWeave::frame(std::string *err) {
  if (!dev_ || !sc_ || lost_) return false;
  if (atlasDirty_ && atlasW_ > 0) {
    waitIdle();
    std::string e2;
    if (uploadR8(atlasPx_.data(), atlasW_, atlasH_, atlas_, &e2))
      atlasDirty_ = false;
    else
      std::fprintf(stderr, "[interfaz] no se pudo subir el atlas: %s\n", e2.c_str());
  }
  if (!atlas_.valid()) { /* el HUD siempre necesita una textura valida, aunque no haya texto */
    static const uint8_t blanco[1] = {255};
    std::string e3;
    uploadR8(blanco, 1, 1, atlas_, &e3);
  }
  fillUbo(uboData_);
  cornersDirty_ = false;
  if (hud_.size() > kMaxHudVerts) hud_.resize(kMaxHudVerts);

  /* hueco que nos toca: el semáforo y el buffer de mando van asociados al hueco, no a la imagen */
  size_t slotIdx = frameNo_ % slots_.size();
  FrameSlot &s = slots_[slotIdx];
  /* primero esperamos el cuadro anterior de este hueco: asi el semaforo que le pasamos a
     vkAcquireNextImageKHR esta libre (VUID-vkAcquireNextImageKHR-semaphore-01779) */
  vkWaitForFences(dev_->device(), 1, &s.fence, VK_TRUE, UINT64_MAX);
  vkResetFences(dev_->device(), 1, &s.fence);

  uint32_t idx = 0;
  VkResult r = vkAcquireNextImageKHR(dev_->device(), sc_, UINT64_MAX, s.sem, VK_NULL_HANDLE, &idx);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    lost_ = true;
    return false;
  }
  if (r != VK_SUCCESS) {
    if (err) *err = std::string("vkAcquireNextImageKHR: ") + vkResultName(r);
    if (r == VK_ERROR_SURFACE_LOST_KHR) lost_ = true;
    return false;
  }
  if (idx >= imgs_.size()) return false;

  /* entra la fuente nueva; la anterior se suelta sólo si ningún otro hueco la está usando */
  if (next_.valid()) keep_.push_back({next_, (int)(gen_++)}), next_ = VkImg{};
  int prevGen = s.gen;
  int useGen = keep_.empty() ? -1 : keep_.back().gen;
  s.gen = useGen;
  if (prevGen >= 0) {
    bool otros = false;
    for (size_t i = 0; i < slots_.size(); i++)
      if ((int)i != (int)slotIdx && slots_[i].gen == prevGen) otros = true;
    if (!otros) {
      for (size_t i = 0; i < keep_.size(); i++)
        if (keep_[i].gen == prevGen) {
          dev_->destroyImg(keep_[i].img);
          keep_.erase(keep_.begin() + (long)i);
          break;
        }
    }
  }
  VkImageView srcView = black_.view;
  for (const Keep &k : keep_)
    if (k.gen == useGen) srcView = k.img.view;

  /* descriptores del hueco */
  writeWeaveSet(s.weave, srcView, s.ubo);
  writeHudSet(s.hud, atlas_.view);

  /* parámetros y geometría, escritos en la memoria mapeada del hueco (ya esperamos su cerca) */
  {
    void *p = nullptr;
    if (vkMapMemory(dev_->device(), s.uboMem, 0, kUboBytes, 0, &p) == VK_SUCCESS) {
      memcpy(p, uboData_, kUboBytes);
      vkUnmapMemory(dev_->device(), s.uboMem);
    }
    if (!hud_.empty() &&
        vkMapMemory(dev_->device(), s.vboMem, 0, (VkDeviceSize)hud_.size() * sizeof(HudVertex), 0, &p) == VK_SUCCESS) {
      memcpy(p, hud_.data(), hud_.size() * sizeof(HudVertex));
      vkUnmapMemory(dev_->device(), s.vboMem);
    }
  }

  VkCommandBuffer cb = s.cmd;
  vkResetCommandBuffer(cb, 0);
  VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(cb, &cbi) != VK_SUCCESS) {
    if (err) *err = "vkBeginCommandBuffer falló";
    return false;
  }
  barrier(cb, imgs_[idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

  VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  att.imageView = views_[idx];
  att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ri.renderArea = {{0, 0}, ext_};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &att;

  VkViewport vpc{0.0f, 0.0f, (float)ext_.width, (float)ext_.height, 0.0f, 1.0f};
  VkRect2D scis{{0, 0}, ext_};
  vkCmdSetViewport(cb, 0, 1, &vpc);
  vkCmdSetScissor(cb, 0, 1, &scis);

  /* pasada 1: la trama, un triángulo a pantalla completa */
  vkCmdBeginRendering(cb, &ri);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, weavePipe_);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, weaveLay_, 0, 1, &s.weave, 0, nullptr);
  vkCmdDraw(cb, 3, 1, 0, 0);
  vkCmdEndRendering(cb);

  /* pasada 2: la interfaz encima, conservando lo dibujado */
  if (!hud_.empty() && atlas_.valid()) {
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT, 1, &mb, 0, nullptr, 0, nullptr);
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    vkCmdBeginRendering(cb, &ri);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, hudLay_, 0, 1, &s.hud, 0, nullptr);
    float pc[2] = {(float)ext_.width, (float)ext_.height};
    vkCmdPushConstants(cb, hudLay_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof pc, pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &s.vbo, &off);
    vkCmdDraw(cb, (uint32_t)hud_.size(), 1, 0, 0);
    vkCmdEndRendering(cb);
  }

  barrier(cb, imgs_[idx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0);
  if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
    if (err) *err = "vkEndCommandBuffer falló";
    return false;
  }

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo sub{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  sub.waitSemaphoreCount = 1;
  sub.pWaitSemaphores = &s.sem;
  sub.pWaitDstStageMask = &waitStage;
  sub.commandBufferCount = 1;
  sub.pCommandBuffers = &cb;
  r = vkQueueSubmit(dev_->queue(), 1, &sub, s.fence);
  if (r != VK_SUCCESS) {
    if (err) *err = std::string("vkQueueSubmit: ") + vkResultName(r);
    return false;
  }
  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.swapchainCount = 1;
  pi.pSwapchains = &sc_;
  pi.pImageIndices = &idx;
  r = vkQueuePresentKHR(dev_->queue(), &pi);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    lost_ = true;
    return false;
  }
  if (r != VK_SUCCESS) {
    if (err) *err = std::string("vkQueuePresentKHR: ") + vkResultName(r);
    return false;
  }
  frameNo_++;
  return true;
}

/* Cuadro fuera de pantalla. Dibuja la trama y la interfaz en una imagen del tamaño del panel y
   la copia a la CPU. No presenta nada: sirve para --dump y para comparar el trazado de Vulkan
   con el de OpenGL cuadro a cuadro. */
bool VkWeave::grab(std::vector<uint8_t> &out, std::string *err, bool copiaPura) {
  if (!dev_ || pw_ <= 0 || ph_ <= 0) {
    if (err) *err = "el trazador no está listo";
    return false;
  }
  waitIdle();
  if (!atlas_.valid()) {
    static const uint8_t blanco[1] = {255};
    uploadR8(blanco, 1, 1, atlas_, nullptr);
  }
  fillUbo(uboData_);
  if (copiaPura) {
    /* el pase de copia del sombreador saca la fuente tal cual, sin campo de la lente ni trama:
       asi se ve si un cuadro plano viene de la imagen que entra o de los calculos de la trama */
    ((float *)(uboData_ + kOffTun4))[3] = 1.0f;
  }
  cornersDirty_ = false;

  VkExtent2D ext{(uint32_t)pw_, (uint32_t)ph_};
  VkImg tgt{};
  if (!dev_->createImage(pw_, ph_, fmt_,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, tgt, err))
    return false;

  const VkDeviceSize pxBytes = (VkDeviceSize)pw_ * (VkDeviceSize)ph_ * 4u;
  VkBuffer stage = VK_NULL_HANDLE, ubo = VK_NULL_HANDLE, vbo = VK_NULL_HANDLE;
  VkDeviceMemory stageMem = VK_NULL_HANDLE, uboMem = VK_NULL_HANDLE, vboMem = VK_NULL_HANDLE;
  bool ok = dev_->createBuffer(pxBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stage,
                               stageMem, err);
  ok = ok && dev_->uploadBytes(uboData_, kUboBytes, ubo, uboMem, err);
  HudVertex vacio{};
  const void *hv = hud_.empty() ? (const void *)&vacio : (const void *)hud_.data();
  VkDeviceSize hbytes = hud_.empty() ? sizeof(HudVertex) : (VkDeviceSize)hud_.size() * sizeof(HudVertex);
  ok = ok && dev_->uploadBytes(hv, hbytes, vbo, vboMem, err);

  VkImageView srcView = keep_.empty() ? black_.view : keep_.back().img.view;
  VkDescriptorSet wset = ok ? newSet(srcSetLay_) : VK_NULL_HANDLE;
  VkDescriptorSet hset = ok ? newSet(hudSetLay_) : VK_NULL_HANDLE;
  if (ok && (!wset || !hset)) {
    if (err) *err = "el charco de descriptores no da para un cuadro fuera de pantalla";
    ok = false;
  }
  if (ok) {
    writeWeaveSet(wset, srcView, ubo);
    writeHudSet(hset, atlas_.view);
  }

  VkCommandBuffer cb = ok ? dev_->beginOneTime(err) : VK_NULL_HANDLE;
  if (!cb) ok = false;
  if (ok) {
    barrier(cb, tgt.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    att.imageView = tgt.view;
    att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, ext};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &att;

    VkViewport vpc{0.0f, 0.0f, (float)ext.width, (float)ext.height, 0.0f, 1.0f};
    VkRect2D scis{{0, 0}, ext};
    vkCmdSetViewport(cb, 0, 1, &vpc);
    vkCmdSetScissor(cb, 0, 1, &scis);

    vkCmdBeginRendering(cb, &ri);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, weavePipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, weaveLay_, 0, 1, &wset, 0, nullptr);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRendering(cb);

    if (!hud_.empty() && atlas_.valid()) {
      VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
      mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      mb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1, &mb, 0,
                           nullptr, 0, nullptr);
      att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      vkCmdBeginRendering(cb, &ri);
      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipe_);
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, hudLay_, 0, 1, &hset, 0, nullptr);
      float pc[2] = {(float)ext.width, (float)ext.height};
      vkCmdPushConstants(cb, hudLay_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof pc, pc);
      VkDeviceSize off = 0;
      vkCmdBindVertexBuffers(cb, 0, 1, &vbo, &off);
      vkCmdDraw(cb, (uint32_t)hud_.size(), 1, 0, 0);
      vkCmdEndRendering(cb);
    }

    barrier(cb, tgt.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy cp{};
    cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    cp.imageExtent = {ext.width, ext.height, 1};
    vkCmdCopyImageToBuffer(cb, tgt.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stage, 1, &cp);
    if (!dev_->endOneTime(cb, err)) ok = false;
  }

  if (ok) {
    void *p = nullptr;
    if (vkMapMemory(dev_->device(), stageMem, 0, pxBytes, 0, &p) != VK_SUCCESS) {
      if (err) *err = "no se pudo leer el cuadro";
      ok = false;
    } else {
      out.assign((size_t)pxBytes, 0);
      const uint8_t *src = (const uint8_t *)p;
      /* el intercambio del panel es B8G8R8A8: lo pasamos a RGBA para el PNG */
      const bool bgra = (fmt_ == VK_FORMAT_B8G8R8A8_UNORM || fmt_ == VK_FORMAT_B8G8R8A8_SRGB);
      for (size_t i = 0; i < (size_t)pxBytes; i += 4) {
        if (bgra) {
          out[i] = src[i + 2];
          out[i + 1] = src[i + 1];
          out[i + 2] = src[i];
        } else {
          out[i] = src[i];
          out[i + 1] = src[i + 1];
          out[i + 2] = src[i + 2];
        }
        out[i + 3] = 255;
      }
      vkUnmapMemory(dev_->device(), stageMem);
    }
  }

  /* liberar lo prestado */
  if (dev_) {
    VkDescriptorSet par[2] = {wset, hset};
    if (wset && hset) vkFreeDescriptorSets(dev_->device(), pool_, 2, par);
    if (stage) vkDestroyBuffer(dev_->device(), stage, nullptr);
    if (stageMem) vkFreeMemory(dev_->device(), stageMem, nullptr);
    if (ubo) vkDestroyBuffer(dev_->device(), ubo, nullptr);
    if (uboMem) vkFreeMemory(dev_->device(), uboMem, nullptr);
    if (vbo) vkDestroyBuffer(dev_->device(), vbo, nullptr);
    if (vboMem) vkFreeMemory(dev_->device(), vboMem, nullptr);
    dev_->destroyImg(tgt);
  }
  return ok;
}

/* ---------------- conversor de video normal a 3D ----------------
   El modelo de profundidad trabaja con una copia pequena de la imagen. Reducirla aqui, en el
   propio GPU, cuesta casi nada y evita bajar los 33 MB del cuadro completo: baja un MB. */

bool VkWeave::prepareConverter(int sz, std::string *err) {
  if (!dev_) {
    if (err) *err = "el trazador no está listo";
    return false;
  }
  if (sz < 16) sz = 16;
  if (small_.valid() && smallW_ == sz) return true;
  waitIdle();
  std::string e2;
  if (!dev_->createImage(sz, sz, fmt_,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, small_, &e2)) {
    if (err) *err = "imagen pequena del conversor: " + e2;
    return false;
  }
  const VkDeviceSize pxBytes = (VkDeviceSize)sz * (VkDeviceSize)sz * 4u;
  if (!dev_->createBuffer(pxBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, smallStage_,
                          smallStageMem_, &e2)) {
    if (err) *err = "buffer de paso del conversor: " + e2;
    return false;
  }
  /* los parametros de este pase fijo son siempre los mismos: ventana del tamano de la copia y
     trama apagada (en ese modo el programa pasa la imagen entera, sin mitades) */
  uint8_t ub[kUboBytes];
  memset(ub, 0, sizeof ub);
  float *res = (float *)(ub + kOffRes);
  res[0] = (float)sz, res[1] = (float)sz;
  ((float *)(ub + kOffTun4))[3] = 1.0f;   /* copia pura desde el principio */
  (void)0;   /* los parametros finos se ponen en readSmall, cuadro a cuadro */
  if (!dev_->uploadBytes(ub, sizeof ub, smallUbo_, smallUboMem_, &e2)) {
    if (err) *err = "buffer uniforme del conversor: " + e2;
    return false;
  }
  smallSet_ = newSet(srcSetLay_);
  if (!smallSet_) {
    if (err) *err = "no hay sitio en el charco de descriptores para el conversor";
    return false;
  }
  writeWeaveSet(smallSet_, black_.view, smallUbo_);   /* dejarlos escritos desde el principio */
  smallW_ = sz, smallH_ = sz;
  return true;
}

bool VkWeave::readSmall(std::vector<uint8_t> &rgba, std::string *err) {
  if (!dev_ || !small_.valid() || !smallSet_) {
    if (err) *err = "el conversor no está preparado";
    return false;
  }
  /* leemos en la CPU: primero nos aseguramos de que la tarjeta no esté usando nada de esto */
  waitIdle();
  /* los mismos parametros del pase grande, pero con la ventana de la copia, la trama apagada y el
     conversor apagado: asi la copia sale limpia y con la misma orientacion que la fuente */
  {
    uint8_t ub[kUboBytes];
    memcpy(ub, uboData_, sizeof ub);
    float *res = (float *)(ub + kOffRes);
    res[0] = (float)smallW_, res[1] = (float)smallH_;
    float *t2 = (float *)(ub + kOffTun2);
    t2[2] = 0.0f;
    float *t4 = (float *)(ub + kOffTun4);
    t4[0] = 0.0f;
    t4[3] = 1.0f;   /* copia pura: sin campo de la lente ni trama */
    void *q = nullptr;
    if (vkMapMemory(dev_->device(), smallUboMem_, 0, kUboBytes, 0, &q) != VK_SUCCESS) {
      if (err) *err = "no se pudo actualizar los parametros de la copia pequena";
      return false;
    }
    memcpy(q, ub, kUboBytes);
    vkUnmapMemory(dev_->device(), smallUboMem_);
  }
  /* la foto que le toca a la red es la misma que se va a dibujar: puede ser un cuadro que todavia
     no paso por frame() (next_) o el ultimo que si paso (keep_) */
  const VkImg *fuente = next_.valid() ? &next_ : (!keep_.empty() ? &keep_.back().img : &black_);
  const VkImageView srcView = fuente->view;
  /* escribimos el juego completo de enlaces, no solo la imagen: las correcciones y el mapa de
     profundidad cambian con el tiempo y un enlace sin escribir deja leer basura en los parametros */
  writeWeaveSet(smallSet_, srcView, smallUbo_);
  const VkDeviceSize pxBytes = (VkDeviceSize)smallW_ * (VkDeviceSize)smallH_ * 4u;
  VkCommandBuffer cb = dev_->beginOneTime(err);
  if (!cb) return false;
  barrier(cb, small_.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  att.imageView = small_.view;
  att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ri.renderArea = {{0, 0}, {(uint32_t)smallW_, (uint32_t)smallH_}};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &att;
  VkViewport vpc{0.0f, 0.0f, (float)smallW_, (float)smallH_, 0.0f, 1.0f};
  VkRect2D scis{{0, 0}, {(uint32_t)smallW_, (uint32_t)smallH_}};
  vkCmdSetViewport(cb, 0, 1, &vpc);
  vkCmdSetScissor(cb, 0, 1, &scis);
  vkCmdBeginRendering(cb, &ri);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, weavePipe_);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, weaveLay_, 0, 1, &smallSet_, 0, nullptr);
  vkCmdDraw(cb, 3, 1, 0, 0);
  vkCmdEndRendering(cb);
  barrier(cb, small_.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  VkBufferImageCopy cp{};
  cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  cp.imageExtent = {(uint32_t)smallW_, (uint32_t)smallH_, 1};
  vkCmdCopyImageToBuffer(cb, small_.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, smallStage_, 1, &cp);
  if (!dev_->endOneTime(cb, err)) return false;

  void *p = nullptr;
  if (vkMapMemory(dev_->device(), smallStageMem_, 0, pxBytes, 0, &p) != VK_SUCCESS) {
    if (err) *err = "no se pudo leer la copia pequena";
    return false;
  }
  rgba.resize((size_t)pxBytes);
  const uint8_t *src = (const uint8_t *)p;
  const bool bgra = (fmt_ == VK_FORMAT_B8G8R8A8_UNORM || fmt_ == VK_FORMAT_B8G8R8A8_SRGB);
  for (size_t i = 0; i < (size_t)pxBytes; i += 4) {
    if (bgra) {
      rgba[i] = src[i + 2];
      rgba[i + 1] = src[i + 1];
      rgba[i + 2] = src[i];
    } else {
      rgba[i] = src[i];
      rgba[i + 1] = src[i + 1];
      rgba[i + 2] = src[i + 2];
    }
    rgba[i + 3] = 255;
  }
  vkUnmapMemory(dev_->device(), smallStageMem_);
  return true;
}

bool VkWeave::setDepthMap(const uint8_t *g, int w, int h, std::string *err) {
  if (!dev_ || !g || w <= 0 || h <= 0) {
    if (err) *err = "mapa de profundidad vacio";
    return false;
  }
  waitIdle();   /* el mapa anterior puede estar en manos de un cuadro en vuelo */
  const VkDeviceSize n = (VkDeviceSize)w * (VkDeviceSize)h;
  const VkDeviceSize nrg = n * 2;

  /* Antes de subir nada, cada ojo recibe su propio mapa: el desplazamiento deparael deja detras
     del canto de un objeto un hueco sin color (en el programa de Samsung lo tapan warp.least_hole,
     warp.hole_edge e inpaint.push/pull). Aqui se rellena el MAPA en el sitio de destino y con la
     guarda de que gana la superficie mas cercana, que es lo que quita el borde babbeado. Ver
     src/hoyos.cpp y tools/prueba_hoyos.cpp, que lo mide sin mirar el panel.
     ODYSSEYVK_HOYOS=0 lo apaga: los dos canales llevan entonces el mapa en crudo, que es
     exactamente lo que hacia la v0.35. */
  static const bool conHoyos = hoyosActivo();
  mapRG_.resize((size_t)nrg);
  int huecos = 0;
  const auto tHoyos0 = std::chrono::steady_clock::now();
  if (conHoyos) {
    /* convGain_ esta en unidades de la pantalla; multiplicado por el ancho del mapa pasa a
       puntos del propio mapa, que es la medida en la que esparcimos */
    rellenarVistas(g, w, convGain_ * (float)w, convRef_, mapRG_.data(), &huecos);
  } else {
    for (VkDeviceSize i = 0; i < n; i++) { mapRG_[2 * i] = g[i]; mapRG_[2 * i + 1] = g[i]; }
  }
  msHoyos_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tHoyos0).count();
  if (entornoOdyssey("ODYSSEYVK_MEDIR") && (++mapasContados_ % 30 == 1))
    std::fprintf(stderr, "[medir] huecos rellenados %d de %lld puntos (%.2f%%) en %.1f ms\n", huecos,
                 (long long)(2 * n), 100.0 * (double)huecos / (double)(2 * n), msHoyos_);
  if (!depth_.valid() || depthW_ != w || depthH_ != h) {
    /* la primera vez, o si cambia el tamano del modelo: imagen nueva y buffer de paso a su medida */
    if (mapStage_) {
      vkDestroyBuffer(dev_->device(), mapStage_, nullptr);
      vkFreeMemory(dev_->device(), mapStageMem_, nullptr);
      mapStage_ = VK_NULL_HANDLE;
      mapStageMem_ = VK_NULL_HANDLE;
    }
    if (!uploadR8(mapRG_.data(), w, h, depth_, err, VK_FORMAT_R8G8_UNORM, 2)) return false;
    if (!dev_->createBuffer(nrg, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mapStage_,
                            mapStageMem_, err))
      return false;
    depthW_ = w, depthH_ = h;
  } else {
    /* y ya solo movemos bytes dentro de la misma imagen: crear y borrar una imagen cada cuadro
       acabó dejando a la tarjeta sin memoria de dispositivo */
    void *p = nullptr;
    if (vkMapMemory(dev_->device(), mapStageMem_, 0, nrg, 0, &p) != VK_SUCCESS) {
      if (err) *err = "no se pudo preparar el mapa de profundidad";
      return false;
    }
    memcpy(p, mapRG_.data(), (size_t)nrg);
    vkUnmapMemory(dev_->device(), mapStageMem_);
    VkCommandBuffer cb = dev_->beginOneTime(err);
    if (!cb) return false;
    barrier(cb, depth_.img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy reg{};
    reg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    reg.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyBufferToImage(cb, mapStage_, depth_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
    barrier(cb, depth_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    if (!dev_->endOneTime(cb, err)) return false;
  }
  depthUsable_ = true;
  return true;
}
