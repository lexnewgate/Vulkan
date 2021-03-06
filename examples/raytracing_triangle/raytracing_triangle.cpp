/*
 * Copyright (C) 2016-2017 by Sascha Willems - www.saschawillems.de
 * Copyright (C) 2019 by Xu Xing - xu.xing@outlook.com
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT) Code is based on Sascha Willems's Vulkan
 * example:
 * https://github.com/SaschaWillems/Vulkan/tree/master/examples/raytracing
 *
 * 3D1 Example - Compute shader ray tracing
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanTexture.hpp"
#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

#if defined(__ANDROID__)
#define TEX_DIM 1024
#else
#define TEX_DIM 2048
#endif

#define USE_TRIANGLES

class VulkanExample : public VulkanExampleBase {
 public:
  vks::Texture textureComputeTarget;

  // Resources for the graphics part of the example
  struct {
    // Raytraced image display shader binding layout
    VkDescriptorSetLayout descriptorSetLayout;
    // Raytraced image display shader bindings
    // before compute shader image manipulation
    VkDescriptorSet descriptorSetPreCompute;
    // Raytraced image display shader bindings
    // after compute shader image manipulation
    VkDescriptorSet descriptorSet;
    // Raytraced image display pipeline
    VkPipeline pipeline;
    // Layout of the graphics pipeline
    VkPipelineLayout pipelineLayout;
  } graphics;

  // Resources for the compute part of the example
  struct {
    struct {
      // (Shader) storage buffer object with scene triangles
      vks::Buffer spheres;
      // (Shader) storage buffer object with scene planes
      vks::Buffer planes;
      // (Shader) storage buffer object with scene spheres
      vks::Buffer triangles;
    } storageBuffers;
    // Uniform buffer object containing scene data
    vks::Buffer uniformBuffer;
    // Separate queue for compute commands (queue family may
    // differ from the one used for graphics)
    VkQueue queue;
    // Use a separate command pool (queue family may
    // differ from the one used for graphics)
    VkCommandPool commandPool;
    // Command buffer storing the dispatch
    // commands and barriers
    VkCommandBuffer commandBuffer;
    // Synchronization fence to avoid rewriting compute CB if
    // still in use
    VkFence fence;
    // Compute shader binding layout
    VkDescriptorSetLayout descriptorSetLayout;
    // Compute shader bindings
    VkDescriptorSet descriptorSet;
    // Layout of the compute pipeline
    VkPipelineLayout pipelineLayout;
    // Compute raytracing pipeline
    VkPipeline pipeline;
    // Compute shader uniform block object
    struct UBOCompute {
      glm::vec3 lightPos;
      // Aspect ratio of the viewport
      float aspectRatio;
      glm::vec4 fogColor = glm::vec4(0.0f);
      struct {
        glm::vec3 pos = glm::vec3(0.0f, 0.0f, 4.0f);
        glm::vec3 lookat = glm::vec3(0.0f, 0.5f, 0.0f);
        float fov = 10.0f;
      } camera;
    } ubo;
  } compute;

  // SSBO triangle declaration
  struct Triangle {
    // Shader uses std140 layout (so we only use vec4 instead of vec3)
    glm::vec3 v0;
    int v0_pad;
    glm::vec3 v1;
    int v1_pad;
    glm::vec3 v2;
    // Id used to identify sphere for raytracing
    uint32_t id;
    glm::vec3 normal;
    float distance;
    glm::vec3 diffuse;
    float specular;
  };

  // SSBO plane declaration
  struct Plane {
    glm::vec3 normal;
    float distance;
    glm::vec3 diffuse;
    float specular;
    uint32_t id;
    glm::ivec3 _pad;
  };

  VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION) {
    title = "Compute shader ray tracing";
    settings.overlay = true;
    viewportWidth = 720;
    viewportHeight = 720;
    compute.ubo.aspectRatio = (float)viewportWidth / (float)viewportHeight;
    timerSpeed *= 0.25f;
    /*
    camera.type = Camera::CameraType::lookat;
    camera.setPerspective(60.0f, (float)viewportWidth / (float)viewportHeight,
    0.1f, 512.0f); camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    camera.setTranslation(glm::vec3(0.0f, 0.0f, -4.0f));
    camera.rotationSpeed = 0.0f;
    camera.movementSpeed = 2.5f;
    */
  }

  ~VulkanExample() {
    // Graphics
    vkDestroyPipeline(device, graphics.pipeline, nullptr);
    vkDestroyPipelineLayout(device, graphics.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, graphics.descriptorSetLayout, nullptr);

    // Compute
    vkDestroyPipeline(device, compute.pipeline, nullptr);
    vkDestroyPipelineLayout(device, compute.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, compute.descriptorSetLayout, nullptr);
    vkDestroyFence(device, compute.fence, nullptr);
    vkDestroyCommandPool(device, compute.commandPool, nullptr);
    compute.uniformBuffer.destroy();
    compute.storageBuffers.spheres.destroy();
    compute.storageBuffers.planes.destroy();
    compute.storageBuffers.triangles.destroy();
    textureComputeTarget.destroy();
  }

  // Prepare a texture target that is used to store compute shader calculations
  void prepareTextureTarget(vks::Texture* tex,
                            uint32_t width,
                            uint32_t height,
                            VkFormat format) {
    // Get device properties for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format,
                                        &formatProperties);
    // Check if requested image format supports image storage operations
    assert(formatProperties.optimalTilingFeatures &
           VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

    // Prepare blit target texture
    tex->width = width;
    tex->height = height;

    VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = {width, height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Image will be sampled in the fragment shader and used as storage target
    // in the compute shader
    imageCreateInfo.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageCreateInfo.flags = 0;

    VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    VK_CHECK_RESULT(
        vkCreateImage(device, &imageCreateInfo, nullptr, &tex->image));
    vkGetImageMemoryRequirements(device, tex->image, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(
        vkAllocateMemory(device, &memAllocInfo, nullptr, &tex->deviceMemory));
    VK_CHECK_RESULT(
        vkBindImageMemory(device, tex->image, tex->deviceMemory, 0));

    VkCommandBuffer layoutCmd = VulkanExampleBase::createCommandBuffer(
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    tex->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    vks::tools::setImageLayout(layoutCmd, tex->image, VK_IMAGE_ASPECT_COLOR_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED, tex->imageLayout);

    VulkanExampleBase::flushCommandBuffer(layoutCmd, queue, true);

    // Create sampler
    VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0f;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.minLod = 0.0f;
    sampler.maxLod = 0.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &tex->sampler));

    // Create image view
    VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                       VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    view.image = tex->image;
    VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &tex->view));

    // Initialize a descriptor for later use
    tex->descriptor.imageLayout = tex->imageLayout;
    tex->descriptor.imageView = tex->view;
    tex->descriptor.sampler = tex->sampler;
    tex->device = vulkanDevice;
  }

  void buildCommandBuffers() {
    // Destroy command buffers if already present
    if (!checkCommandBuffers()) {
      destroyCommandBuffers();
      createCommandBuffers();
    }

    VkCommandBufferBeginInfo cmdBufInfo =
        vks::initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBeginInfo =
        vks::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = viewportWidth;
    renderPassBeginInfo.renderArea.extent.height = viewportHeight;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
      // Set target frame buffer
      renderPassBeginInfo.framebuffer = frameBuffers[i];

      VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

      // Image memory barrier to make sure that compute shader writes are
      // finished before sampling from the texture
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.image = textureComputeTarget.image;
      imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                             1};
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(drawCmdBuffers[i],
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_FLAGS_NONE,
                           0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

      vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo,
                           VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = vks::initializers::viewport(
          (float)viewportWidth, (float)viewportHeight, 0.0f, 1.0f);
      vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

      VkRect2D scissor =
          vks::initializers::rect2D(viewportWidth, viewportHeight, 0, 0);
      vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

      // Display ray traced image generated by compute shader as a full screen
      // quad Quad vertices are generated in the vertex shader
      vkCmdBindDescriptorSets(
          drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
          graphics.pipelineLayout, 0, 1, &graphics.descriptorSet, 0, NULL);
      vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                        graphics.pipeline);
      vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

      // drawUI(drawCmdBuffers[i]);

      vkCmdEndRenderPass(drawCmdBuffers[i]);

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
  }

  void buildComputeCommandBuffer() {
    VkCommandBufferBeginInfo cmdBufInfo =
        vks::initializers::commandBufferBeginInfo();

    VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

    vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      compute.pipeline);
    vkCmdBindDescriptorSets(
        compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0, 0);

    vkCmdDispatch(compute.commandBuffer, textureComputeTarget.width / 16,
                  textureComputeTarget.height / 16, 1);

    vkEndCommandBuffer(compute.commandBuffer);
  }
  // Id used to identify objects by the ray tracing shader
  uint32_t currentId = 0;

  Triangle newTriangle(glm::vec3 v0,
                       glm::vec3 v1,
                       glm::vec3 v2,
                       glm::vec3 normal,
                       float distance,
                       glm::vec3 diffuse,
                       float specular) {
    Triangle triangle;
    triangle.id = currentId++;
    triangle.distance = distance;
    triangle.diffuse = diffuse;
    triangle.specular = specular;
    triangle.normal = normal;
    triangle.v0 = v0;
    triangle.v1 = v1;
    triangle.v2 = v2;
    return triangle;
  }

  Plane newPlane(glm::vec3 normal,
                 float distance,
                 glm::vec3 diffuse,
                 float specular) {
    Plane plane;
    plane.id = currentId++;
    plane.normal = normal;
    plane.distance = distance;
    plane.diffuse = diffuse;
    plane.specular = specular;
    return plane;
  }

  // Setup and fill the compute shader storage buffers containing primitives for
  // the raytraced scene
  void prepareStorageBuffers() {
    VkDeviceSize storageBufferSize;
    // Stage
    vks::Buffer stagingBuffer;
    VkCommandBuffer copyCmd;
    VkBufferCopy copyRegion = {};

#ifdef USE_TRIANGLES
    // Triangles
    std::vector<Triangle> triangles;
    triangles.push_back(newTriangle(
        glm::vec3(-1.0f, -1.0f, -4.0f), glm::vec3(1.0f, -1.0f, -4.0f),
        glm::vec3(0.0f, 1.0f, -4.0f), glm::vec3(0.0f, 0.0f, 1.0f), 4.0f,
        glm::vec3(0.0f, 1.0f, 0.0f), 32.0f));

    triangles.push_back(
        newTriangle(glm::vec3(1.0f, 1.0f, -4.0f), glm::vec3(0.0f, 1.0f, -4.0f),
                    glm::vec3(1.0f, 0.0f, -4.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                    4.0f, glm::vec3(1.0f, 0.0f, 0.0f), 32.0f));

    storageBufferSize = triangles.size() * sizeof(Triangle);

    vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               &stagingBuffer, storageBufferSize,
                               triangles.data());

    vulkanDevice->createBuffer(
        // The SSBO will be used as a storage buffer for the compute pipeline
        // and as a vertex buffer in the graphics pipeline
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &compute.storageBuffers.spheres,
        storageBufferSize);

    // Copy to staging buffer
    copyCmd = VulkanExampleBase::createCommandBuffer(
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    copyRegion = {};
    copyRegion.size = storageBufferSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer,
                    compute.storageBuffers.spheres.buffer, 1, &copyRegion);
    VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

    stagingBuffer.destroy();
#endif

  }

  void setupDescriptorPool() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        // Compute UBO
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              2),
        // Graphics image samplers
        vks::initializers::descriptorPoolSize(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4),
        // Storage image for ray traced image output
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                              1),
        // Storage buffer for the scene primitives
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                              1),
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo =
        vks::initializers::descriptorPoolCreateInfo(poolSizes.size(),
                                                    poolSizes.data(), 3);

    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr,
                                           &descriptorPool));
  }

  void setupDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0 : Fragment shader image sampler
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT, 0)};

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
        vks::initializers::descriptorSetLayoutCreateInfo(
            setLayoutBindings.data(), setLayoutBindings.size());

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
        device, &descriptorLayout, nullptr, &graphics.descriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
        vks::initializers::pipelineLayoutCreateInfo(
            &graphics.descriptorSetLayout, 1);

    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo,
                                           nullptr, &graphics.pipelineLayout));
  }

  void setupDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo =
        vks::initializers::descriptorSetAllocateInfo(
            descriptorPool, &graphics.descriptorSetLayout, 1);

    VK_CHECK_RESULT(
        vkAllocateDescriptorSets(device, &allocInfo, &graphics.descriptorSet));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        // Binding 0 : Fragment shader texture sampler
        vks::initializers::writeDescriptorSet(
            graphics.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0, &textureComputeTarget.descriptor)};

    vkUpdateDescriptorSets(device, writeDescriptorSets.size(),
                           writeDescriptorSets.data(), 0, NULL);
  }

  void preparePipelines() {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        vks::initializers::pipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        vks::initializers::pipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
        vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        vks::initializers::pipelineColorBlendStateCreateInfo(
            1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        vks::initializers::pipelineDepthStencilStateCreateInfo(
            VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
        vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        vks::initializers::pipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT, 0);

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState =
        vks::initializers::pipelineDynamicStateCreateInfo(
            dynamicStateEnables.data(), dynamicStateEnables.size(), 0);

    // Display pipeline
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    shaderStages[0] = loadShader(
        getAssetPath() + "shaders/raytracing_triangle/texture.vert.spv",
        VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(
        getAssetPath() + "shaders/raytracing_triangle/texture.frag.spv",
        VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo =
        vks::initializers::pipelineCreateInfo(graphics.pipelineLayout,
                                              renderPass, 0);

    VkPipelineVertexInputStateCreateInfo emptyInputState{};
    emptyInputState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    emptyInputState.vertexAttributeDescriptionCount = 0;
    emptyInputState.pVertexAttributeDescriptions = nullptr;
    emptyInputState.vertexBindingDescriptionCount = 0;
    emptyInputState.pVertexBindingDescriptions = nullptr;
    pipelineCreateInfo.pVertexInputState = &emptyInputState;

    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.renderPass = renderPass;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1,
                                              &pipelineCreateInfo, nullptr,
                                              &graphics.pipeline));
  }

  // Prepare the compute pipeline that generates the ray traced image
  void prepareCompute() {
    // Create a compute capable device queue
    // The VulkanDevice::createLogicalDevice functions finds a compute capable
    // queue and prefers queue families that only support compute Depending on
    // the implementation this may result in different queue family indices for
    // graphics and computes, requiring proper synchronization (see the memory
    // barriers in buildComputeCommandBuffer)
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = NULL;
    queueCreateInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
    queueCreateInfo.queueCount = 1;
    vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.compute, 0,
                     &compute.queue);

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0: Storage image (raytraced output)
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        // Binding 1: Uniform buffer block
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
#ifdef USE_TRIANGLES
        // Binding 2: Shader storage buffer for the spheres
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
#endif
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
        vks::initializers::descriptorSetLayoutCreateInfo(
            setLayoutBindings.data(), setLayoutBindings.size());

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
        device, &descriptorLayout, nullptr, &compute.descriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
        vks::initializers::pipelineLayoutCreateInfo(
            &compute.descriptorSetLayout, 1);

    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo,
                                           nullptr, &compute.pipelineLayout));

    VkDescriptorSetAllocateInfo allocInfo =
        vks::initializers::descriptorSetAllocateInfo(
            descriptorPool, &compute.descriptorSetLayout, 1);

    VK_CHECK_RESULT(
        vkAllocateDescriptorSets(device, &allocInfo, &compute.descriptorSet));

    std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
        // Binding 0: Output storage image
        vks::initializers::writeDescriptorSet(
            compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0,
            &textureComputeTarget.descriptor),
        // Binding 1: Uniform buffer block
        vks::initializers::writeDescriptorSet(
            compute.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            &compute.uniformBuffer.descriptor),
#ifdef USE_TRIANGLES
        // Binding 2: Shader storage buffer for the spheres
        vks::initializers::writeDescriptorSet(
            compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2,
            &compute.storageBuffers.spheres.descriptor),
#endif

#ifdef USE_PLANES
		// Binding 3: Shader storage buffer for the planes
        vks::initializers::writeDescriptorSet(
            compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3,
            &compute.storageBuffers.planes.descriptor)
#endif
    };

    vkUpdateDescriptorSets(device, computeWriteDescriptorSets.size(),
                           computeWriteDescriptorSets.data(), 0, NULL);

    // Create compute shader pipelines
    VkComputePipelineCreateInfo computePipelineCreateInfo =
        vks::initializers::computePipelineCreateInfo(compute.pipelineLayout, 0);

    computePipelineCreateInfo.stage = loadShader(
        getAssetPath() + "shaders/raytracing_triangle/raytracing.comp.spv",
        VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1,
                                             &computePipelineCreateInfo,
                                             nullptr, &compute.pipeline));

    // Separate command pool as queue family for compute may be different than
    // graphics
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr,
                                        &compute.commandPool));

    // Create a command buffer for compute operations
    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        vks::initializers::commandBufferAllocateInfo(
            compute.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo,
                                             &compute.commandBuffer));

    // Fence for compute CB sync
    VkFenceCreateInfo fenceCreateInfo =
        vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK_RESULT(
        vkCreateFence(device, &fenceCreateInfo, nullptr, &compute.fence));

    // Build a single command buffer containing the compute dispatch commands
    buildComputeCommandBuffer();
  }

  // Prepare and initialize uniform buffer containing shader uniforms
  void prepareUniformBuffers() {
    // Compute shader parameter uniform buffer block
    vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               &compute.uniformBuffer, sizeof(compute.ubo));

    updateUniformBuffers();
  }

  void updateUniformBuffers() {
#if 1
    // sin(glm::radians(timer * 360.0f)) *
    compute.ubo.lightPos.x = 0.0f;
    // cos(glm::radians(timer * 360.0f)) * 2.0f;
    compute.ubo.lightPos.y = 0.0f;
    // sin(glm::radians(timer * 360.0f)) * 2.0f;
    compute.ubo.lightPos.z = 0.0f;  // cos(glm::radians(timer * 360.0f)) * 2.0f;
    compute.ubo.camera.pos = glm::vec3(0.0f, -0.0f, 0.0f);
    VK_CHECK_RESULT(compute.uniformBuffer.map());
    memcpy(compute.uniformBuffer.mapped, &compute.ubo, sizeof(compute.ubo));
    compute.uniformBuffer.unmap();
#endif
  }

  void draw() {
    VulkanExampleBase::prepareFrame();

    // Command buffer to be sumitted to the queue
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

    VulkanExampleBase::submitFrame();

    // Submit compute commands
    // Use a fence to ensure that compute command buffer has finished executing
    // before using it again
    vkWaitForFences(device, 1, &compute.fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &compute.fence);

    VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;

    VK_CHECK_RESULT(
        vkQueueSubmit(compute.queue, 1, &computeSubmitInfo, compute.fence));
  }

  void prepare() {
    VulkanExampleBase::prepare();
    prepareStorageBuffers();
    prepareUniformBuffers();
    prepareTextureTarget(&textureComputeTarget, TEX_DIM, TEX_DIM,
                         VK_FORMAT_R8G8B8A8_UNORM);
    setupDescriptorSetLayout();
    preparePipelines();
    setupDescriptorPool();
    setupDescriptorSet();
    prepareCompute();
    buildCommandBuffers();
    prepared = true;
  }

  virtual void render() {
    if (!prepared)
      return;
    draw();
    if (!paused) {
      updateUniformBuffers();
    }
  }

  virtual void viewChanged() {
    compute.ubo.aspectRatio = (float)viewportWidth / (float)viewportHeight;
    updateUniformBuffers();
  }
};

VULKAN_EXAMPLE_MAIN()
