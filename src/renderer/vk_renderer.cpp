#include <vulkan/vulkan.h>
#include <platform.h>
#include <defines.h>
#include <dds_structs.h>
#ifdef WINDOWS_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif
#include <iostream>
#include <renderer\vk_init.cpp>
#include <renderer\vk_types.h>

#define VK_CHECK(result)                                      \
    if (result != VK_SUCCESS)                                 \
    {                                                         \
        std::cout << "Vulkan Error: " << result << std::endl; \
        __debugbreak();                                       \
        return false;                                         \
    }

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    std::cout << "Validation Error: " << pCallbackData->pMessage << std::endl;
    return false;
}

struct VkContext
{
    VkExtent2D screenSize;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkCommandBuffer cmd;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkRenderPass renderpass;
    VkCommandPool commandPool;
    VkSurfaceFormatKHR surfaceFormat;
    VkSemaphore submitSemaphore;
    VkFence imgAvailableFence;
    VkSemaphore aquireSemaphore;

    VkPipeline pipeline;
    VkPipelineLayout pipeLayout;
    uint32_t scImgCount;
    // TODO: Suballocation from main memory
    VkImage scImages[5];
    VkImageView scImageViews[5];
    VkFramebuffer framebuffers[5];
    // TODO will be inside array
    Image image;
    Buffer stagingBuffer;
    uint32_t graphicsIdx;
};

bool vk_init(VkContext *vkcontext, void *window)
{
    platform_get_window_size(&vkcontext->screenSize.width, &vkcontext->screenSize.height);

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong";
    appInfo.pEngineName = "Ponggine";

    char *extensions[]{
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME};

    char *layers[]{
        "VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledExtensionCount = ArraySize(extensions);
    instanceInfo.ppEnabledLayerNames = layers;
    instanceInfo.enabledLayerCount = ArraySize(layers);
    VK_CHECK(vkCreateInstance(&instanceInfo, 0, &vkcontext->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback = vk_debug_callback;
        vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, 0, &vkcontext->debugMessenger);
    }
    else
    {
        return false;
    }

    // Create surface
    {
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
#elif LINUX_BUILD
#endif
    }

    // Choose GPU
    {
        vkcontext->graphicsIdx = -1;
        uint32_t gpuCount = 0;
        //  TODO: Suballocation from main allocation
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];

            uint32_t queueFamilyCount = 0;
            //  TODO: Suballocation from main allocation
            VkQueueFamilyProperties queueProps[10];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProps);

            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                if (queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 surfaceSupport = VK_FALSE;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupport));

                    if (surfaceSupport)
                    {
                        vkcontext->graphicsIdx = j;
                        vkcontext->gpu = gpu;
                        break;
                    }
                }
            }
        }

        if (vkcontext->graphicsIdx < 0)
        {
            return false;
        }
    }

    // Logical Device
    {
        float queuePriority = 1.0;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueCount = 1;
        queueInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        queueInfo.pQueuePriorities = &queuePriority;

        char *extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.enabledExtensionCount = ArraySize(extensions);

        VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, 0, &vkcontext->device));

        vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIdx, 0, &vkcontext->graphicsQueue);
    }

    // Swap Chain
    {
        uint32_t formatCount = 0;
        // TODO: Suballocation from main memory
        VkSurfaceFormatKHR surfaceFormats[10];
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, 0));
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, surfaceFormats));

        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            if (format.format = VK_FORMAT_B8G8R8A8_SRGB)
            {
                vkcontext->surfaceFormat = format;
                break;
            }
        }

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps));
        uint32_t imgCount = 0;
        imgCount = surfaceCaps.minImageCount + 1;
        imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.surface = vkcontext->surface;
        scInfo.imageFormat = vkcontext->surfaceFormat.format;
        scInfo.preTransform = surfaceCaps.currentTransform;
        scInfo.imageExtent = surfaceCaps.currentExtent;
        scInfo.minImageCount = imgCount;
        scInfo.imageArrayLayers = 1;
        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, 0, &vkcontext->swapchain));

        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, vkcontext->scImages));

        // Create the image views
        {
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.format = vkcontext->surfaceFormat.format;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;

            for (uint32_t i = 0; i < vkcontext->scImgCount; i++)
            {
                viewInfo.image = vkcontext->scImages[i];

                VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &vkcontext->scImageViews[i]));
            }
        }
    }

    // Render Pass
    {
        VkAttachmentDescription attachment = {};
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.format = vkcontext->surfaceFormat.format;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc = {};
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorAttachmentRef;

        VkAttachmentDescription attachments[]{
            attachment,
        };
        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments = attachments;
        rpInfo.attachmentCount = ArraySize(attachments);
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpassDesc;
        VK_CHECK(vkCreateRenderPass(vkcontext->device, &rpInfo, 0, &vkcontext->renderpass));
    }

    // Framebuffers
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.width = vkcontext->screenSize.width;
        fbInfo.height = vkcontext->screenSize.height;
        fbInfo.renderPass = vkcontext->renderpass;
        fbInfo.layers = 1;
        fbInfo.attachmentCount = 1;
        for (uint32_t i = 0; i < vkcontext->scImgCount; i++)
        {
            fbInfo.pAttachments = &vkcontext->scImageViews[i];
            vkCreateFramebuffer(vkcontext->device, &fbInfo, 0, &vkcontext->framebuffers[i]);
        }
    }

    // Pipeline Layout
    {
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        VK_CHECK(vkCreatePipelineLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->pipeLayout));
    }

    // Pipeline
    {
        VkShaderModule vertexShader, fragmentShader;

        // Vertext Shader
        {
            uint32_t sizeInBytes;
            char *vertexCode = platform_read_file("assets/shaders/shader.vert.spv", &sizeInBytes);
            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = (uint32_t *)vertexCode;
            shaderInfo.codeSize = sizeInBytes;
            VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &vertexShader));
            // TODO: Suballocation from main allocation
            delete vertexCode;
        }

        // Fragment Shader]
        {
            uint32_t sizeInBytes;
            char *fragCode = platform_read_file("assets/shaders/shader.frag.spv", &sizeInBytes);
            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = (uint32_t *)fragCode;
            shaderInfo.codeSize = sizeInBytes;
            VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &fragmentShader));
            // TODO: Suballocation from main allocation
            delete fragCode;
        }

        VkPipelineShaderStageCreateInfo vertexStage = {};
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.pName = "main";
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexShader;

        VkPipelineShaderStageCreateInfo fragStage = {};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.pName = "main";
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragmentShader;
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertexStage,
            fragStage};

        VkPipelineVertexInputStateCreateInfo vertexInputState = {};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState colorAttatchment = {};
        colorAttatchment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorAttatchment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.pAttachments = &colorAttatchment;
        colorBlendState.attachmentCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth = 1.0f;

        VkRect2D scissor = {};
        VkViewport viewport = {};

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        viewportState.pViewports = &viewport;
        viewportState.viewportCount = 1;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamicStates;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates);

        VkGraphicsPipelineCreateInfo pipeInfo = {};
        pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeInfo.layout = vkcontext->pipeLayout;
        pipeInfo.renderPass = vkcontext->renderpass;
        pipeInfo.pColorBlendState = &colorBlendState;
        pipeInfo.pVertexInputState = &vertexInputState;
        pipeInfo.pStages = shaderStages;
        pipeInfo.stageCount = ArraySize(shaderStages);
        pipeInfo.pRasterizationState = &rasterizationState;
        pipeInfo.pViewportState = &viewportState;
        pipeInfo.pDynamicState = &dynamicState;
        pipeInfo.pMultisampleState = &multisampleState;
        pipeInfo.pInputAssemblyState = &inputAssembly;
        VK_CHECK(vkCreateGraphicsPipelines(vkcontext->device, 0, 1, &pipeInfo, 0, &vkcontext->pipeline));
    }

    // Command Pool
    {

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        VK_CHECK(vkCreateCommandPool(vkcontext->device, &poolInfo, 0, &vkcontext->commandPool));
    }

    // Command Buffer
    {
        VkCommandBufferAllocateInfo allocInfo = cmd_alloc_info(vkcontext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &vkcontext->cmd));
    }

    // Sync Objects
    {
        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->aquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->submitSemaphore));

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &vkcontext->imgAvailableFence));
    }
    // Staging Buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.size = MB(1);
        VK_CHECK(vkCreateBuffer(vkcontext->device, &bufferInfo, 0, &vkcontext->stagingBuffer.buffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vkcontext->device, vkcontext->stagingBuffer.buffer, &memRequirements);

        VkPhysicalDeviceMemoryProperties gpuMemProps;
        vkGetPhysicalDeviceMemoryProperties(vkcontext->gpu, &gpuMemProps);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = MB(1);
        for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
        {
            if (memRequirements.memoryTypeBits & (1 << i) &&
                (gpuMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
                    (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
            {
                allocInfo.memoryTypeIndex = i;
            }
        }

        VK_CHECK(vkAllocateMemory(vkcontext->device, &allocInfo, 0, &vkcontext->stagingBuffer.memory));
        VK_CHECK(vkMapMemory(vkcontext->device, vkcontext->stagingBuffer.memory, 0, MB(1), 0, &vkcontext->stagingBuffer.data));
        VK_CHECK(vkBindBufferMemory(vkcontext->device, vkcontext->stagingBuffer.buffer, vkcontext->stagingBuffer.memory, 0));
    }

    // Create Image
    {
        uint32_t fileSize = 0;
        DDSFile *data = (DDSFile *)platform_read_file("assets/textures/cakez.DDS", &fileSize);
        uint32_t textureSize = data->header.Width * data->header.Height * 4;
        memcpy(vkcontext->stagingBuffer.data, &data->dataBegin, textureSize);

        // TODO Assertions
        VkImageCreateInfo imgInfo = {};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgInfo.extent = {data->header.Width, data->header.Height, 1};
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        // imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(vkcontext->device, &imgInfo, 0, &vkcontext->image.image));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(vkcontext->device, vkcontext->image.image, &memRequirements);

        VkPhysicalDeviceMemoryProperties gpuMemProps;
        vkGetPhysicalDeviceMemoryProperties(vkcontext->gpu, &gpuMemProps);
        VkMemoryAllocateInfo allocInfo = {};
        for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
        {
            if (memRequirements.memoryTypeBits & (1 << i) &&
                (gpuMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                allocInfo.memoryTypeIndex = i;
            }
        }

        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = textureSize;

        VK_CHECK(vkAllocateMemory(vkcontext->device, &allocInfo, 0, &vkcontext->image.memory));
        VK_CHECK(vkBindImageMemory(vkcontext->device, vkcontext->image.image, vkcontext->image.memory, 0));

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAlloc = cmd_alloc_info(vkcontext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &cmdAlloc, &cmd));

        VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        VkBufferImageCopy copyRegion = {};
        copyRegion.imageExtent = {data->header.Width, data->header.Height, 1};
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdCopyBufferToImage(cmd, vkcontext->stagingBuffer.buffer, vkcontext->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VK_CHECK(vkEndCommandBuffer(cmd));
        // shoulld be 9/6 :D
        vkDeviceWaitIdle(vkcontext->device);
    }

    return true;
}

bool vk_render(VkContext *vkcontext)
{
    uint32_t imgIdx;
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, 0, vkcontext->aquireSemaphore, 0, &imgIdx));
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = vkcontext->commandPool;
    VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkClearValue clearValue = {};
    clearValue.color = {1, 1, 0, 1};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = vkcontext->renderpass;
    rpBeginInfo.renderArea.extent = vkcontext->screenSize;
    rpBeginInfo.framebuffer = vkcontext->framebuffers[imgIdx];
    rpBeginInfo.pClearValues = &clearValue;
    rpBeginInfo.clearValueCount = 1;
    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Rendering Commands
    {
        VkRect2D scissor = {};
        scissor.extent = vkcontext->screenSize;

        VkViewport viewport = {};
        viewport.height = vkcontext->screenSize.height;
        viewport.width = vkcontext->screenSize.width;
        viewport.maxDepth = 1.0;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkcontext->aquireSemaphore;
    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, 0));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkcontext->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imgIdx;
    presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo));

    VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

    vkFreeCommandBuffers(vkcontext->device, vkcontext->commandPool, 1, &cmd);

    return true;
}