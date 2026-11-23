/* vkprobe.cpp — ver vkprobe.h. */
#include "vkprobe.h"

#include "fs_vert_sp.h"     /* generado por CMake desde shaders/fs.vert   */
#include "probe_frag_sp.h"  /* generado por CMake desde shaders/probe.frag */

#include <cstring>
#include <vector>

#define VKTRY(call)                                                  \
  do {                                                               \
    VkResult r_ = (call);                                            \
    if (r_ != VK_SUCCESS) {                                          \
      if (err) *err = std::string(#call) + ": " + vkResultName(r_);   \
      return false;                                                  \
    }                                                                \
  } while (0)

bool vkPassToRgba(VkDev &dev, const VkPass &pass, std::vector<uint8_t> &rgba, std::string *err) {
  if (!dev.ok() || !pass.vert || !pass.frag || pass.outW <= 0 || pass.outH <= 0) {
    if (err) *err = "paso Vulkan incompleto";
    return false;
  }
  VkDevice d = dev.device();

  /* ---- cosas que hay que deshacer al salir, pase lo que pase ---- */
  VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
  VkPipeline pipe = VK_NULL_HANDLE;
  VkDescriptorSet set = VK_NULL_HANDLE;
  bool setAllocated = false;
  VkBuffer uboBuf = VK_NULL_HANDLE;
  VkDeviceMemory uboMem = VK_NULL_HANDLE;
  VkImg out{};
  VkBuffer outBuf = VK_NULL_HANDLE;
  VkDeviceMemory outMem = VK_NULL_HANDLE;
  auto cleanup = [&]() {
    if (outBuf) vkDestroyBuffer(d, outBuf, nullptr);
    if (outMem) vkFreeMemory(d, outMem, nullptr);
    dev.destroyImg(out);
    if (uboBuf) vkDestroyBuffer(d, uboBuf, nullptr);
    if (uboMem) vkFreeMemory(d, uboMem, nullptr);
    if (pipe) vkDestroyPipeline(d, pipe, nullptr);
    if (pipeLayout) vkDestroyPipelineLayout(d, pipeLayout, nullptr);
    if (setAllocated) vkFreeDescriptorSets(d, dev.descPool(), 1, &set);
    if (setLayout) vkDestroyDescriptorSetLayout(d, setLayout, nullptr);
    if (vs) vkDestroyShaderModule(d, vs, nullptr);
    if (fs) vkDestroyShaderModule(d, fs, nullptr);
  };
  auto fail = [&](const std::string &s) {
    if (err) *err = s;
    cleanup();
    return false;
  };

  VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  smi.codeSize = pass.vertLen;
  smi.pCode = reinterpret_cast<const uint32_t *>(pass.vert);
  VKTRY(vkCreateShaderModule(d, &smi, nullptr, &vs));
  smi.codeSize = pass.fragLen;
  smi.pCode = reinterpret_cast<const uint32_t *>(pass.frag);
  VKTRY(vkCreateShaderModule(d, &smi, nullptr, &fs));

  if (!dev.ensurePool(8, err)) {
    cleanup();
    return false;
  }

  VkDescriptorSetLayoutBinding b[2]{};
  b[0].binding = 0;
  b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  b[0].descriptorCount = 1;
  b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  b[1].binding = 1;
  b[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  b[1].descriptorCount = 1;
  b[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dli.bindingCount = 2;
  dli.pBindings = b;
  VKTRY(vkCreateDescriptorSetLayout(d, &dli, nullptr, &setLayout));

  /* bloque uniforme: los datos del llamador, o un bloque a cero si no trae nada */
  size_t uboN = pass.uboSize ? pass.uboSize : (size_t)128;
  std::vector<uint8_t> ceros;
  const void *uboDatos = pass.ubo;
  if (!uboDatos) {
    ceros.assign(uboN, 0);
    uboDatos = ceros.data();
  }
  if (!dev.uploadBytes(uboDatos, uboN, uboBuf, uboMem, err)) return fail("no se pudo subir el bloque uniforme");

  VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dsai.descriptorPool = dev.descPool();
  dsai.descriptorSetCount = 1;
  dsai.pSetLayouts = &setLayout;
  VKTRY(vkAllocateDescriptorSets(d, &dsai, &set));
  setAllocated = true;

  VkDescriptorImageInfo dii{};
  dii.sampler = dev.sampler();
  dii.imageView = pass.src;
  /* una imagen importada con modificador DRM vive siempre en layout GENERAL */
  dii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  VkDescriptorBufferInfo dbi{};
  dbi.buffer = uboBuf;
  dbi.offset = 0;
  dbi.range = uboN;
  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = set;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[0].pImageInfo = &dii;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = set;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[1].pBufferInfo = &dbi;
  vkUpdateDescriptorSets(d, pass.src != VK_NULL_HANDLE ? 2u : 1u, writes, 0, nullptr);

  if (!dev.createImage(pass.outW, pass.outH, pass.format,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, out, err))
    return fail("no se pudo crear la imagen de salida");

  VkDeviceSize outBytes = (VkDeviceSize)pass.outW * (VkDeviceSize)pass.outH * 4u;
  if (!dev.createBuffer(outBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, outBuf, outMem,
                        err))
    return fail("no se pudo crear el búfer de lectura");

  /* ---- tubería de trazado con representación dinámica (Vulkan 1.3) ---- */
  VkPipelineRenderingCreateInfo pri{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  VkFormat fmt = pass.format;
  pri.colorAttachmentCount = 1;
  pri.pColorAttachmentFormats = &fmt;

  VkGraphicsPipelineCreateInfo gpi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gpi.pNext = &pri;
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fs;
  stages[1].pName = "main";
  gpi.stageCount = 2;
  gpi.pStages = stages;
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  gpi.pVertexInputState = &vi;
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = pass.topo;
  gpi.pInputAssemblyState = &ia;
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.scissorCount = 1;
  gpi.pViewportState = &vp;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.0f;
  gpi.pRasterizationState = &rs;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  gpi.pMultisampleState = &ms;
  VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  gpi.pDepthStencilState = &ds;
  VkPipelineColorBlendAttachmentState cba{};
  cba.colorWriteMask = 0xf;
  VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cbs.attachmentCount = 1;
  cbs.pAttachments = &cba;
  gpi.pColorBlendState = &cbs;
  VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dsi{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dsi.dynamicStateCount = 2;
  dsi.pDynamicStates = dyn;
  gpi.pDynamicState = &dsi;
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pli.setLayoutCount = 1;
  pli.pSetLayouts = &setLayout;
  VKTRY(vkCreatePipelineLayout(d, &pli, nullptr, &pipeLayout));
  gpi.layout = pipeLayout;
  VkResult pr = vkCreateGraphicsPipelines(d, VK_NULL_HANDLE, 1, &gpi, nullptr, &pipe);
  if (pr != VK_SUCCESS) {
    std::string det = std::string("vkCreateGraphicsPipelines falló: ") + vkResultName(pr) +
                      " (vértices " + std::to_string(pass.vertLen) + " bytes · fragmento " +
                      std::to_string(pass.fragLen) + " bytes · topología " + std::to_string((int)pass.topo) +
                      " · formato de salida " + VkDev::vkFormatName(fmt) + ")";
    return fail(det);
  }

  VkCommandBuffer cb = dev.beginOneTime(err);
  if (cb == VK_NULL_HANDLE) return fail("no se pudo grabar la lista de comandos");

  /* layouts: fuente a GENERAL, salida a COLOR_ATTACHMENT */
  VkImageMemoryBarrier bars[2]{};
  uint32_t nbar = 0;
  if (pass.srcImage != VK_NULL_HANDLE) {
    bars[nbar].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bars[nbar].srcAccessMask = 0;
    bars[nbar].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bars[nbar].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bars[nbar].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    bars[nbar].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bars[nbar].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bars[nbar].image = pass.srcImage;
    bars[nbar].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    nbar++;
  }
  bars[nbar].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  bars[nbar].srcAccessMask = 0;
  bars[nbar].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  bars[nbar].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  bars[nbar].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  bars[nbar].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bars[nbar].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bars[nbar].image = out.img;
  bars[nbar].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  nbar++;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                       nullptr, 0, nullptr, nbar, bars);

  VkClearValue cv{};
  cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkRenderingAttachmentInfo att{};
  att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  att.imageView = out.view;
  att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.clearValue = cv;
  VkRenderingInfo ri{};
  ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea = {{0, 0}, {(uint32_t)pass.outW, (uint32_t)pass.outH}};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &att;
  vkCmdBeginRendering(cb, &ri);
  VkViewport vpt{0.0f, 0.0f, (float)pass.outW, (float)pass.outH, 0.0f, 1.0f};
  VkRect2D sc{{0, 0}, {(uint32_t)pass.outW, (uint32_t)pass.outH}};
  vkCmdSetViewport(cb, 0, 1, &vpt);
  vkCmdSetScissor(cb, 0, 1, &sc);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, 0, 1, &set, 0, nullptr);
  vkCmdDraw(cb, (uint32_t)pass.vertices, 1, 0, 0);
  vkCmdEndRendering(cb);

  VkImageMemoryBarrier toCopy{};
  toCopy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toCopy.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  toCopy.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  toCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toCopy.image = out.img;
  toCopy.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &toCopy);

  VkBufferImageCopy reg{};
  reg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  reg.imageExtent = {(uint32_t)pass.outW, (uint32_t)pass.outH, 1};
  vkCmdCopyImageToBuffer(cb, out.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outBuf, 1, &reg);

  if (!dev.endOneTime(cb, err)) return fail("la GPU no terminó el trazado");

  void *p = nullptr;
  if (vkMapMemory(d, outMem, 0, outBytes, 0, &p) != VK_SUCCESS) return fail("no se pudo leer el resultado");
  rgba.assign((size_t)outBytes, 0);
  std::memcpy(rgba.data(), p, (size_t)outBytes);
  vkUnmapMemory(d, outMem);
  cleanup();
  return true;
}

bool vkProbeFrame(VkDev &dev, const GpuFrame &f, std::vector<uint8_t> &rgba, std::string &info, std::string *err) {
  VkFormat fmt = VK_FORMAT_UNDEFINED;
  bool consultaOk = dev.canImport(f, &fmt, err);
  if (!consultaOk) {
    /* La consulta formal de NVIDIA dice que no admite handles externos, pero el driver
       suele aceptar el fd igualmente: lo intentamos y que hable la API. */
    info += "aviso: " + (err && !err->empty() ? *err : std::string("la consulta dijo que no")) + "\n";
    if (err) err->clear();
    fmt = VkDev::vkFormatFor(f.fourcc);
    if (fmt == VK_FORMAT_UNDEFINED) fmt = VK_FORMAT_B8G8R8A8_UNORM;
  }
  info += "cuatrocc " + fourccText(f.fourcc) + " -> " + std::string(VkDev::vkFormatName(fmt)) + "\n";
  info += "tamaño " + std::to_string(f.w) + "x" + std::to_string(f.h) + " · paso " + std::to_string(f.stride) +
          " · desplazamiento " + std::to_string(f.offset) + "\n";
  info += "modificador " + VkDev::modifierName(f.modifier) + "\n";

  VkImg src{};
  bool importado = dev.importImage(f, src, err, consultaOk);
  if (!importado) {
    info += "con modificador no se pudo: " + (err && !err->empty() ? *err : std::string("motivo desconocido")) + "\n";
    if (err) err->clear();
    dev.destroyImg(src);
    importado = dev.importImageImplicit(f, src, &info, err);
  }
  if (!importado) return false;
  info += "imagen Vulkan creada sobre el DMA-BUF (sin copia por CPU)\n";

  struct Info {
    float res[2];
  } ubo{(float)f.w, (float)f.h};

  VkPass pass;
  pass.vert = fsVertSpv;
  pass.vertLen = fsVertSpv_len;
  pass.frag = probeFragSpv;
  pass.fragLen = probeFragSpv_len;
  pass.src = src.view;
  pass.srcImage = src.img;
  pass.ubo = &ubo;
  pass.uboSize = sizeof ubo;
  pass.outW = f.w;
  pass.outH = f.h;
  pass.vertices = 3;
  pass.format = fmt;

  bool ok = vkPassToRgba(dev, pass, rgba, err);
  dev.destroyImg(src);
  if (ok)
    info += "muestreo correcto: " + std::to_string(f.w) + "x" + std::to_string(f.h) + " pixels leídos de la GPU\n";
  return ok;
}
