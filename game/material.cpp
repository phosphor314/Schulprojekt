#include "material.h"
#include "BufferStructs.h"
#include "constants.h"

MaterialType Material::getType() const { return material; }

VkPipelineLayout Material::getPipelineLayout() const { return pipelineLayout; }

void Material::beginMaterialPass(VkCommandBuffer commandBuffer) const {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

Material::Material(MaterialType type, RenderEngine &engine,
                   VkDescriptorSetLayout layout) {
  material = type;
  switch (type) {
  case ENEMIES:
    createEnemiesMaterial(engine, layout);
    break;
  case PARTICLES:
    createParticlesMaterial(engine, layout);
    break;
  case BULLETS:
    createBulletsMaterial(engine, layout);
    break;
  default:
    throw std::runtime_error("MAX_MAT ist not a material");
  }
}

void Material::free(VkDevice dev) {
  vkDestroyPipeline(dev, pipeline, nullptr);
  for (int i=1; i < selfLayouts.size(); ++i) {
    vkDestroyDescriptorSetLayout(dev, selfLayouts[i], nullptr);
  }
  vkDestroyPipelineLayout(dev, pipelineLayout, nullptr);
}

void Material::createEnemiesSetLayouts(RenderEngine &engine) {}

void Material::createEnemiesMaterial(RenderEngine &engine,
                                     VkDescriptorSetLayout layout) {
  {
    selfLayouts.push_back(layout);
    createEnemiesSetLayouts(engine);
    VkPipelineLayoutCreateInfo cInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    cInfo.pSetLayouts = selfLayouts.data();
    cInfo.setLayoutCount = selfLayouts.size();
    VkVerify(vkCreatePipelineLayout(engine.device, &cInfo, nullptr,
                                    &pipelineLayout));
  }
  {
    auto vertShaderCode = engine.readFile(SHADER_ROOT + "compiled/vert.spv");
    VkShaderModule vertShaderModule = engine.createShaderModule(vertShaderCode);
    auto fragShaderCode = engine.readFile(SHADER_ROOT + "compiled/frag.spv");
    VkShaderModule fragShaderModule = engine.createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    std::array<VkDynamicState, 2> dynamicState{VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateInfo.dynamicStateCount = dynamicState.size();
    dynamicStateInfo.pDynamicStates = dynamicState.data();

    VkVertexInputBindingDescription vertexInputBind;
    vertexInputBind.binding = 0;
    vertexInputBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBind.stride = sizeof(Vertex);

    std::array<VkVertexInputAttributeDescription, 3> vertexInputAttrs;
    vertexInputAttrs[0].binding = 0;
    vertexInputAttrs[0].location = 0;
    vertexInputAttrs[0].offset = offsetof(Vertex, position);
    vertexInputAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;

    vertexInputAttrs[1].binding = 0;
    vertexInputAttrs[1].location = 1;
    vertexInputAttrs[1].offset = offsetof(Vertex, normal);
    vertexInputAttrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;

    vertexInputAttrs[2].binding = 0;
    vertexInputAttrs[2].location = 2;
    vertexInputAttrs[2].offset = offsetof(Vertex, color);
    vertexInputAttrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexInputBind;
    vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttrs.data();
    vertexInputInfo.vertexAttributeDescriptionCount = vertexInputAttrs.size();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // never use primitive topology

    // set in dynamic state
    VkViewport viewport{};
    VkRect2D scissor{};

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    colorBlendInfo.logicOpEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vertStageInfo, fragStageInfo};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = engine.renderPass;
    pipelineInfo.subpass = 0;

    VkVerify(vkCreateGraphicsPipelines(engine.device, VK_NULL_HANDLE, 1,
                                       &pipelineInfo, nullptr, &pipeline));

    vkDestroyShaderModule(engine.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(engine.device, fragShaderModule, nullptr);
  }
}

void Material::createParticleSetLayouts(RenderEngine &engine) {
  {
    std::array<VkDescriptorSetLayoutBinding, 1> bindings;
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo cInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = bindings.size(),
        .pBindings = bindings.data(),
    };

    selfLayouts.push_back(VK_NULL_HANDLE);
    vkCreateDescriptorSetLayout(engine.device, &cInfo, nullptr, selfLayouts.data() + (selfLayouts.size() - 1));
  }
}

void Material::createParticlesMaterial(RenderEngine &engine, VkDescriptorSetLayout layout) {
  {
    VkPipelineLayoutCreateInfo cInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    selfLayouts.push_back(layout);
  	createParticleSetLayouts(engine);
    cInfo.pSetLayouts = selfLayouts.data();
    cInfo.setLayoutCount = selfLayouts.size();
    VkVerify(vkCreatePipelineLayout(engine.device, &cInfo, nullptr, &pipelineLayout));
  }
  
  {
    auto vertShaderCode =
        engine.readFile(SHADER_ROOT + "compiled/particleVert.spv");
    VkShaderModule vertShaderModule = engine.createShaderModule(vertShaderCode);
    auto fragShaderCode =
        engine.readFile(SHADER_ROOT + "compiled/particleFrag.spv");
    VkShaderModule fragShaderModule = engine.createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    std::array<VkDynamicState, 2> dynamicState{VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateInfo.dynamicStateCount = dynamicState.size();
    dynamicStateInfo.pDynamicStates = dynamicState.data();

    VkVertexInputBindingDescription vertexInputBind;
    vertexInputBind.binding = 0;
    vertexInputBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBind.stride = sizeof(glm::vec3);

    std::array<VkVertexInputAttributeDescription, 1> vertexInputAttrs;
    vertexInputAttrs[0].binding = 0;
    vertexInputAttrs[0].location = 0;
    vertexInputAttrs[0].offset = 0;
    vertexInputAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexInputBind;
    vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttrs.data();
    vertexInputInfo.vertexAttributeDescriptionCount = vertexInputAttrs.size();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    // set in dynamic state
    VkViewport viewport{};
    VkRect2D scissor{};

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    colorBlendInfo.logicOpEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vertStageInfo, fragStageInfo};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = engine.renderPass;
    pipelineInfo.subpass = 0;

    VkVerify(vkCreateGraphicsPipelines(engine.device, VK_NULL_HANDLE, 1,
                                       &pipelineInfo, nullptr, &pipeline));

    vkDestroyShaderModule(engine.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(engine.device, fragShaderModule, nullptr);
  }
}

void Material::createBulletsSetLayouts(RenderEngine &) {}
void Material::createBulletsMaterial(RenderEngine &engine,
                                     VkDescriptorSetLayout layout) {
  createBulletsSetLayouts(engine);
  {
    VkPipelineLayoutCreateInfo cInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    selfLayouts.push_back(layout);
    cInfo.pSetLayouts = selfLayouts.data();
    cInfo.setLayoutCount = selfLayouts.size();
    VkVerify(vkCreatePipelineLayout(engine.device, &cInfo, nullptr,
                                    &pipelineLayout));
    selfLayouts.pop_back();
  }
  // bullet pipeline
  {
    auto vertShaderCode =
        engine.readFile(SHADER_ROOT + "compiled/bulletVert.spv");
    VkShaderModule vertShaderModule = engine.createShaderModule(vertShaderCode);
    auto fragShaderCode =
        engine.readFile(SHADER_ROOT + "compiled/bulletFrag.spv");
    VkShaderModule fragShaderModule = engine.createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    std::array<VkDynamicState, 2> dynamicState{VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateInfo.dynamicStateCount = dynamicState.size();
    dynamicStateInfo.pDynamicStates = dynamicState.data();

    std::array<VkVertexInputBindingDescription, 1> vertexInputBinds;
    vertexInputBinds[0].binding = 0;
    vertexInputBinds[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    vertexInputBinds[0].stride = sizeof(Projectile);

    std::array<VkVertexInputAttributeDescription, 2> vertexInputAttrs;
    vertexInputAttrs[0].binding = 0;
    vertexInputAttrs[0].location = 0;
    vertexInputAttrs[0].offset = offsetof(Projectile, position);
    vertexInputAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;

    vertexInputAttrs[1].binding = 0;
    vertexInputAttrs[1].location = 1;
    vertexInputAttrs[1].offset = offsetof(Projectile, forward);
    vertexInputAttrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = vertexInputBinds.size();
    vertexInputInfo.pVertexBindingDescriptions = vertexInputBinds.data();
    vertexInputInfo.vertexAttributeDescriptionCount = vertexInputAttrs.size();
    vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // set in dynamic state
    VkViewport viewport{};
    VkRect2D scissor{};

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    colorBlendInfo.logicOpEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vertStageInfo, fragStageInfo};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = engine.renderPass;
    pipelineInfo.subpass = 1;

    VkVerify(vkCreateGraphicsPipelines(engine.device, VK_NULL_HANDLE, 1,
                                       &pipelineInfo, nullptr, &pipeline));

    vkDestroyShaderModule(engine.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(engine.device, fragShaderModule, nullptr);
  }
}

MaterialLoader::MaterialLoader(RenderEngine &engine,
                               VkDescriptorSetLayout perFrameSet) {
  for (MaterialType T = (MaterialType)0; T < MAX_MAT;
       T = (MaterialType)((int)T + 1)) {
    materials.emplace_back(T, engine, perFrameSet);
  }
}

const Material &
MaterialLoader::beginMaterialPass(MaterialType type,
                                  VkCommandBuffer commandBuffer) {
  assert(materials[type].getType() == type);
  materials[type].beginMaterialPass(commandBuffer);
  return materials[type];
}

void MaterialLoader::free(VkDevice dev) {
  for (Material &m : materials) {
    m.free(dev);
  }
}
