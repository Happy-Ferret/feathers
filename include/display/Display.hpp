#pragma once

#include <unistd.h>

#include <magma/DisplaySystem.hpp>
#include <magma/VulkanHandler.hpp>
#include <magma/Device.hpp>
#include <magma/Swapchain.hpp>
#include <magma/Fence.hpp>
#include <magma/Semaphore.hpp>
#include <magma/CreateInfo.hpp>
#include <magma/ShaderModule.hpp>
#include <magma/DescriptorSetLayout.hpp>
#include <magma/Buffer.hpp>
#include <magma/DeviceMemory.hpp>
#include <magma/Framebuffer.hpp>
#include <magma/DescriptorSets.hpp>
#include <magma/DescriptorSetLayout.hpp>
#include <magma/Image.hpp>
#include <magma/Sampler.hpp>
#include <magma/DynamicBuffer.hpp>

#include "display/SuperCorbeau.hpp"

namespace display
{
  class Display
  {
    struct Renderer
    {
      struct UserData
      {
	magma::CommandPool<> commandPool;
	magma::DescriptorSetLayout<> descriptorSetLayout;
	magma::PipelineLayout<> pipelineLayout;
	magma::ShaderModule<> vert;
	magma::ShaderModule<> frag;

	UserData(magma::Device<claws::no_delete> device, vk::PhysicalDevice, uint32_t selectedQueueFamily)
	  : commandPool(device.createCommandPool({vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, selectedQueueFamily))
	  , descriptorSetLayout(device.createDescriptorSetLayout({
		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr
		    }}))
	  , pipelineLayout(device.createPipelineLayout({}, {descriptorSetLayout}, {}))
	{
	  {
	    std::ifstream vertSource("spirv/basic.vert.spirv");
	    std::ifstream fragSource("spirv/basic.frag.spirv");

	    if (!vertSource || !fragSource)
	      throw std::runtime_error("Failed to load shaders");
	    vert = device.createShaderModule(static_cast<std::istream &>(vertSource));
	    frag = device.createShaderModule(static_cast<std::istream &>(fragSource));
	  }

	}

	vk::Extent2D getExtent() const noexcept
	{
	  return vk::Extent2D{100, 100};
	}
      };

      struct SwapchainUserData
      {
	magma::CommandBufferGroup<magma::PrimaryCommandBuffer> commandBuffers;
	magma::RenderPass<> renderPass;
	magma::Pipeline<> pipeline;

	// This functions pipeline creation
	// the reason I refactored this out is that it's pretty long and verbose
	magma::Pipeline<> createPipeline(magma::Device<claws::no_delete> device, magma::Swapchain<claws::no_delete> swapchain, UserData const &userData)
	{
	  std::cout << "creating pipeline for swapchain with extent " << swapchain.getExtent().width << ", " << swapchain.getExtent().height << std::endl;
	  /// --- Specialisation info --- ///
	  // The width and height are specialized rather than being push constants, because a compositor shoudn't change size often (If ever).
	  // Both entries have the size of a float
	  // The second entry is offset by one float
	  std::array<vk::SpecializationMapEntry, 2u> mapEntries{
	    vk::SpecializationMapEntry{0, 0, sizeof(float)},
	      vk::SpecializationMapEntry{1, sizeof(float), sizeof(float)}
	  };
	  // These are the actual specialisation values
	  std::array<float, 2u> dimensions{static_cast<float>(swapchain.getExtent().width), static_cast<float>(swapchain.getExtent().height)};

	  auto specialzationInfo(magma::StructBuilder<vk::SpecializationInfo>::make(mapEntries, sizeof(float) * 2, dimensions.data()));
	  /// --- Specialisation info END --- ///

	  // We have two shaders, and both shader's entrypoints are "main".
	  // Only the vertex shader is specialized (see above setup)
	  std::vector<vk::PipelineShaderStageCreateInfo>
	    shaderStageCreateInfos{{{}, vk::ShaderStageFlagBits::eVertex, userData.vert, "main", &specialzationInfo},
	      {{}, vk::ShaderStageFlagBits::eFragment, userData.frag, "main", nullptr}};

	  // We are rendering triangle strips, and primitives shouldn't restart.
	  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleStrip, false};
	  // We have tzo seperate wertex buffers: one for position and one for color
	  std::array<vk::VertexInputBindingDescription, 2u> vertexInputBindings{
	    vk::VertexInputBindingDescription{0, 2 * sizeof(float), vk::VertexInputRate::eVertex},
	      vk::VertexInputBindingDescription{1, 2 * sizeof(float), vk::VertexInputRate::eVertex}
	  };
	  // Both vertex buffers contains `vec2`s of floats, which is the format `vk::Format::eR32G32Sfloat`
	  std::array<vk::VertexInputAttributeDescription, 2u>
	    vertexInputAttrib{vk::VertexInputAttributeDescription{0, vertexInputBindings[0].binding, vk::Format::eR32G32Sfloat, 0},
	      vk::VertexInputAttributeDescription{1, vertexInputBindings[1].binding, vk::Format::eR32G32Sfloat, 0}};

	  auto vertexInputStateCreateInfo(magma::StructBuilder<vk::PipelineVertexInputStateCreateInfo, true>::make(vertexInputBindings,
														   vertexInputAttrib));

	  // The viewport is st up so that the top left corner is (0, 0) and the bottom right (width, height)
	  // We don't really care about depth
	  vk::Viewport viewport(-static_cast<float>(swapchain.getExtent().width),
				-static_cast<float>(swapchain.getExtent().height), // pos
				static_cast<float>(swapchain.getExtent().width) * 2.0f,
				static_cast<float>(swapchain.getExtent().height) * 2.0f, // size
				0.0f,
				1.0); // depth range

	  // Scissors cover all that is within the compositor
	  vk::Rect2D scissor({0, 0}, swapchain.getExtent());

	  auto viewportStateCreateInfo(magma::StructBuilder<vk::PipelineViewportStateCreateInfo, true>::make(magma::asListRef(viewport),
													     magma::asListRef(scissor)));

	  // Everything is turned off
	  vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
	    {},
	      false,                            // VkBool32                                       depthClampEnable
		false,                            // VkBool32                                       rasterizerDiscardEnable
		vk::PolygonMode::eFill,           // VkPolygonMode                                  polygonMode
		vk::CullModeFlagBits::eNone,      // VkCullModeFlags                                cullMode
		vk::FrontFace::eCounterClockwise, // VkFrontFace                                    frontFace
		false,                            // VkBool32                                       depthBiasEnable
		0.0f,                             // float                                          depthBiasConstantFactor
		0.0f,                             // float                                          depthBiasClamp
		0.0f,                             // float                                          depthBiasSlopeFactor
		1.0f                              // float                                          lineWidth
		};

	  // Everything is turned off
	  // We use only one smape per pixel
	  vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
	    {},
	      vk::SampleCountFlagBits::e1, // VkSampleCountFlagBits                          rasterizationSamples
		false,                       // VkBool32                                       sampleShadingEnable
		1.0f,                        // float                                          minSampleShading
		nullptr,                     // const VkSampleMask                            *pSampleMask
		false,                       // VkBool32                                       alphaToCoverageEnable
		false                        // VkBool32                                       alphaToOneEnable
		};

	  // No blending
	  vk::PipelineColorBlendAttachmentState
	    colorBlendAttachmentState{false,                  // VkBool32                                       blendEnable
	      vk::BlendFactor::eOne,  // VkBlendFactor                                  srcColorBlendFactor
	      vk::BlendFactor::eZero, // VkBlendFactor                                  dstColorBlendFactor
	      vk::BlendOp::eAdd,      // VkBlendOp                                      colorBlendOp
	      vk::BlendFactor::eOne,  // VkBlendFactor                                  srcAlphaBlendFactor
	      vk::BlendFactor::eZero, // VkBlendFactor                                  dstAlphaBlendFactor
	      vk::BlendOp::eAdd,      // VkBlendOp                                      alphaBlendOp
	      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
	      | vk::ColorComponentFlagBits::eA};

	  vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
	    {},                 // VkPipelineColorBlendStateCreateFlags           flags
	      false,              // VkBool32                                       logicOpEnable
		vk::LogicOp::eCopy, // VkLogicOp                                      logicOp
		1,
		&colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState     *pAttachments
		  {0.0f, 0.0f, 0.0f, 0.0f}    // float                                          blendConstants[4]
	  };

	  magma::GraphicsPipelineConfig pipelineCreateInfo({},
							   std::move(shaderStageCreateInfos),
							   vertexInputStateCreateInfo,
							   inputAssemblyStateCreateInfo,
							   rasterizationStateCreateInfo,
							   userData.pipelineLayout,
							   renderPass,
							   0);

	  pipelineCreateInfo.addRasteringColorAttachementInfo(viewportStateCreateInfo, multisampleStateCreateInfo, colorBlendStateCreateInfo);
	  return device.createPipeline(pipelineCreateInfo);
	}

	SwapchainUserData() = default;
	SwapchainUserData(magma::Device<claws::no_delete> device, magma::Swapchain<claws::no_delete> swapchain, UserData &userData, uint32_t imageCount)
	  : commandBuffers(userData.commandPool.allocatePrimaryCommandBuffers(imageCount))
	  , renderPass([&](){
	      magma::RenderPassCreateInfo renderPassCreateInfo{{}};

	      // We have a simple renderpass writting to an image.
	      // The attached framebuffer will be cleared
	      renderPassCreateInfo.attachements.push_back({{},
		    swapchain.getFormat(),
		      vk::SampleCountFlagBits::e1,
		      vk::AttachmentLoadOp::eClear,
		      vk::AttachmentStoreOp::eStore,
		      vk::AttachmentLoadOp::eDontCare,
		      vk::AttachmentStoreOp::eDontCare,
		      vk::ImageLayout::eUndefined,
		      vk::ImageLayout::ePresentSrcKHR});

	      vk::AttachmentReference colorAttachmentReferences(0, vk::ImageLayout::eColorAttachmentOptimal);

	      renderPassCreateInfo.subPasses.push_back(magma::StructBuilder<vk::SubpassDescription, true>
						       ::make(vk::PipelineBindPoint::eGraphics,
							      magma::EmptyList{},
							      magma::asListRef(colorAttachmentReferences),
							      nullptr,
							      nullptr,
							      magma::EmptyList{}));

	      return device.createRenderPass(renderPassCreateInfo);
	    }())
	  , pipeline(createPipeline(device, swapchain, userData))
	{
	}
      };

      struct FrameData
      {
	magma::Fence<> fence;
	magma::Framebuffer<> framebuffer;

	FrameData(magma::Device<claws::no_delete> device, magma::Swapchain<claws::no_delete> swapchain, UserData &, SwapchainUserData &swapchainUserData, magma::ImageView<claws::no_delete> swapchainImageView)
	  : fence(device.createFence(vk::FenceCreateFlagBits::eSignaled))
	  , framebuffer(device.createFramebuffer(swapchainUserData.renderPass,
						 std::vector<vk::ImageView>{swapchainImageView},
						 swapchain.getExtent().width,
						 swapchain.getExtent().height,
						 1))
	{
	}
      };
      vk::PhysicalDevice physicalDevice;
      magma::Device<> device;
      magma::Semaphore<> imageAvailable;
      magma::Semaphore<> renderDone;
      vk::Queue queue;
      magma::DisplaySystem<UserData, SwapchainUserData, FrameData> displaySystem;

      magma::Buffer<> quadBuffer;
      magma::DeviceMemory<> quadBufferMemory;
      magma::DescriptorPool<> descriptorPool;
      magma::DescriptorSets<> descriptorSets;
      magma::Image<> backgroundImage;
      magma::DeviceMemory<> backgroundImageMemory;
      magma::ImageView<> backgroundImageView;
      magma::DynamicBuffer stagingBuffer;
      magma::Sampler<> sampler;

      struct Score
      {
	bool isSuitable;
	unsigned int bestQueue;
	vk::PhysicalDeviceType deviceType;

	unsigned int deviceTypeScore() const noexcept
	{
	  switch(deviceType)
	    {
	    case vk::PhysicalDeviceType::eIntegratedGpu:
	      return 4;
	    case vk::PhysicalDeviceType::eDiscreteGpu:
	      return 3;
	    case vk::PhysicalDeviceType::eVirtualGpu:
	      return 2;
	    case vk::PhysicalDeviceType::eOther:
	      return 1;
	    case vk::PhysicalDeviceType::eCpu:
	    default:
	      return 0;
	    }
	}

	bool operator<(Score const &other) const noexcept
	{
	  return (!isSuitable || (other.isSuitable && (deviceTypeScore() < other.deviceTypeScore())));
	}
      };

      Renderer(std::pair<vk::PhysicalDevice, Score> const &selectedResult, magma::Surface<claws::no_delete> surface)
	: physicalDevice(selectedResult.first)
	, device([this, &selectedResult, surface](){
	    float priority{1.0f};
	    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{{}, selectedResult.second.bestQueue, 1, &priority};

	    return magma::Device<>(physicalDevice,
				   std::vector<vk::DeviceQueueCreateInfo>({deviceQueueCreateInfo}),
				   std::vector<char const *>({VK_KHR_SWAPCHAIN_EXTENSION_NAME}));
	  }())
	, imageAvailable(device.createSemaphore())
	, renderDone(device.createSemaphore())
	, queue(device.getQueue(selectedResult.second.bestQueue, 0u))
	, displaySystem(physicalDevice, surface, device, queue, selectedResult.second.bestQueue)
	, quadBuffer(device.createBuffer({}, 8 * sizeof(float), vk::BufferUsageFlagBits::eVertexBuffer, {selectedResult.second.bestQueue}))
	, quadBufferMemory([this](){
	    auto memRequirements(device.getBufferMemoryRequirements(quadBuffer));

	    return device.selectAndCreateDeviceMemory(physicalDevice, memRequirements.size, vk::MemoryPropertyFlagBits::eHostVisible, memRequirements.memoryTypeBits);
	  }())
	, descriptorPool(device.createDescriptorPool(1, {vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1}}))
	, descriptorSets(descriptorPool.allocateDescriptorSets({displaySystem.userData.descriptorSetLayout}))
	, backgroundImage(device.createImage2D({}, vk::Format::eR8G8B8A8Unorm, {display::superCorbeau::width, display::superCorbeau::height}, vk::SampleCountFlagBits::e1,
					       vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::ImageLayout::eUndefined))
	, backgroundImageMemory([this](){
	    auto memRequirements(device.getImageMemoryRequirements(backgroundImage));

	    auto memory(device.selectAndCreateDeviceMemory(physicalDevice, memRequirements.size, vk::MemoryPropertyFlagBits::eHostVisible, memRequirements.memoryTypeBits));
	    device.bindImageMemory(backgroundImage, memory, 0);
	    return memory;
	  }())
	, backgroundImageView(device.createImageView({},
						     backgroundImage,
						     vk::ImageViewType::e2D,
						     vk::Format::eR8G8B8A8Unorm,
						     vk::ComponentMapping{},
						     vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,
									       0,
									       1,
									       0,
									       1)))
	, stagingBuffer(device,
			physicalDevice,
			{},
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible,
			std::vector<uint32_t>{selectedResult.second.bestQueue})
	, sampler(device.createSampler(vk::Filter::eNearest,
				       vk::Filter::eNearest,
				       vk::SamplerMipmapMode::eNearest,
				       vk::SamplerAddressMode::eClampToEdge,
				       vk::SamplerAddressMode::eClampToEdge,
				       vk::SamplerAddressMode::eClampToEdge,
				       0.0f,
				       false,
				       0.0f, // no effect
				       false,
				       vk::CompareOp::eAlways, // no effect
				       0.0f,
				       0.0f,
				       vk::BorderColor::eIntOpaqueWhite,
				       false))
      {
	{
	  magma::DynamicBuffer::RangeId tmpBuffer(stagingBuffer.allocate(display::superCorbeau::width * display::superCorbeau::height * 4));
	  auto memory(stagingBuffer.getMemory<unsigned char []>(tmpBuffer));
	  auto src(display::superCorbeau::header_data);
	  for (unsigned int i(0u); i < display::superCorbeau::width * display::superCorbeau::height; ++i)
	    display::superCorbeau::headerPixel(src, &memory[i * 4]);
	  auto commandBuffers(displaySystem.userData.commandPool.allocatePrimaryCommandBuffers(1));
	  commandBuffers[0].begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	  vk::ImageSubresourceRange imageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

	  commandBuffers[0].raw().pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
						  {
						    vk::ImageMemoryBarrier{
						      {},
							vk::AccessFlagBits::eTransferWrite,
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eTransferDstOptimal,
							  VK_QUEUE_FAMILY_IGNORED,
							  VK_QUEUE_FAMILY_IGNORED,
							  backgroundImage,
							  imageSubresourceRange
							  }
						  });
	  commandBuffers[0].raw().copyBufferToImage(stagingBuffer.getBuffer(tmpBuffer),
						    backgroundImage,
						    vk::ImageLayout::eTransferDstOptimal,
						    {
						      vk::BufferImageCopy{
							tmpBuffer.second,
							  0, 0,
							  vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
							  vk::Offset3D{0, 0, 0},
							    vk::Extent3D{display::superCorbeau::width, display::superCorbeau::height, 1}
						      }
						    });
	  ;
	  commandBuffers[0].raw().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
						  {
						    vk::ImageMemoryBarrier{
						      vk::AccessFlagBits::eTransferWrite,
							vk::AccessFlagBits::eShaderRead,
							vk::ImageLayout::eTransferDstOptimal,
							vk::ImageLayout::eShaderReadOnlyOptimal,
							VK_QUEUE_FAMILY_IGNORED,
							VK_QUEUE_FAMILY_IGNORED,
							backgroundImage,
							imageSubresourceRange
							}
						  });

	  commandBuffers[0].end();
	  magma::Fence<> fence(device.createFence({}));
	  queue.submit(magma::StructBuilder<vk::SubmitInfo>::make(magma::EmptyList(),
								  nullptr,
								  magma::asListRef(commandBuffers[0].raw()),
								  magma::EmptyList()),
		       fence);
	  device.waitForFences({fence}, true, 1000000000);
	}

	vk::DescriptorImageInfo const imageInfo
	{
	  sampler,
	    backgroundImageView,
	    vk::ImageLayout::eShaderReadOnlyOptimal
	    };
	device.updateDescriptorSets(std::array<vk::WriteDescriptorSet, 1u>{vk::WriteDescriptorSet{descriptorSets[0], 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr}});
	device.bindBufferMemory(quadBuffer, quadBufferMemory, 0);
	{
	  auto deleter([&](auto data)
		       {
			 if (data)
			   device.unmapMemory(quadBufferMemory);
		       });
	  std::unique_ptr<float[], decltype(deleter)> quadData(reinterpret_cast<float *>(device.mapMemory(quadBufferMemory, 0, VK_WHOLE_SIZE)),
							       deleter);
	  quadData[0] = 0.0f;
	  quadData[1] = 0.0f;
	  quadData[2] = 100.0f;
	  quadData[3] = 0.0f;
	  quadData[4] = 0.0f;
	  quadData[5] = 100.0f;
	  quadData[6] = 100.0f;
	  quadData[7] = 100.0f;
	}
      }

    public:
      Renderer(magma::Instance const &instance, magma::Surface<claws::no_delete> surface)
	: Renderer([&instance, surface](){
	    std::pair<vk::PhysicalDevice, Score>
	      result(instance.selectDevice([&instance, surface]
					   (vk::PhysicalDevice physicalDevice)
					   {
					     std::vector<vk::QueueFamilyProperties> queueFamilyPropertiesList(physicalDevice.getQueueFamilyProperties());
					     unsigned int bestQueueIndex = 0;
					     for (; bestQueueIndex < queueFamilyPropertiesList.size(); ++bestQueueIndex)
					       {
						 vk::QueueFamilyProperties const &queueFamilyProperties(queueFamilyPropertiesList[bestQueueIndex]);

						 if ((queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) &&
						     surface.isQueueFamilySuitable(physicalDevice, bestQueueIndex))
						   {
						     break;
						   }
					       }
					     vk::PhysicalDeviceProperties properties(physicalDevice.getProperties());
					     return Score{bestQueueIndex != queueFamilyPropertiesList.size(), bestQueueIndex, properties.deviceType};
					   }));
	    if (!result.second.isSuitable)
	      {
		throw std::runtime_error("No suitable GPU found.");
	      }
	    return result;
	  }(), surface)
      {
      }

      ~Renderer() noexcept
      {
	try {
	  quadBuffer = magma::Buffer<>{}; // destroy buffer before memory being free'd
	  backgroundImage = magma::Image<>{}; // destroy image before memory being free'd
	} catch (...) {
	  std::cerr << "swallowing error: failed to destroy quadBuffer\n" << std::endl;
	}
      }

      void render()
      {
	// get next image data, and image to present
	auto [index, frame] = displaySystem.getImage(imageAvailable);

	// wait for rendering fence
	device.waitForFences({frame.fence}, true, 1000000000);
	// reset fence
	device.resetFences({frame.fence});
	magma::PrimaryCommandBuffer cmdBuffer(displaySystem.swapchainUserData.commandBuffers[index]);
	uint32_t const vertexCount(4u);

	// being command recording
	cmdBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	// we bind are quad buffer to both bindings
	cmdBuffer.bindVertexBuffers(0, {quadBuffer}, {0ul});
	cmdBuffer.bindVertexBuffers(1, {quadBuffer}, {0ul});
	// we bind update our descriptor so that it points to our image
	cmdBuffer.raw().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, displaySystem.userData.pipelineLayout, 0, 1, descriptorSets.data(), 0, nullptr);
	vk::ClearValue clearValue = {vk::ClearColorValue(std::array<float, 4>{0.0f, 0.5f, 0.0f, 1.0f})}; // a nice recognisable green for debug
	{
	  // start the renderpass
	  auto lock(cmdBuffer.beginRenderPass(displaySystem.swapchainUserData.renderPass, frame.framebuffer,
					      {{0, 0}, displaySystem.getSwapchain().getExtent()}, {clearValue}, vk::SubpassContents::eInline));

	  // us our pipeline
	  lock.bindGraphicsPipeline(displaySystem.swapchainUserData.pipeline);
	  // draw our quad
	  lock.draw(vertexCount, 1, 0, 0);
	}
	cmdBuffer.end();

	vk::PipelineStageFlags waitDestStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	queue.submit(magma::StructBuilder<vk::SubmitInfo>::make(asListRef(imageAvailable), // wait fo image to be available
								&waitDestStageMask,
								magma::asListRef(cmdBuffer.raw()),
								magma::asListRef(renderDone)), // signal renderdone when done
		     frame.fence); // signal the fence
	//std::cout << "about to present for index " << index << std::endl;
	displaySystem.presentImage(renderDone, index); // present our image
      }
    };

    magma::Instance instance;
    magma::Surface<> surface;
    Renderer renderer;

  public:
    template<class SurfaceProvider>
    Display(SurfaceProvider &surfaceProvider)
      : instance{SurfaceProvider::getRequiredExtensions()}
      , surface(surfaceProvider.createSurface(instance))
      , renderer(instance, surface)
    {
    }

    Display(Display const &) = delete;
    Display(Display &&) = delete;
    Display operator=(Display const &) = delete;
    Display operator=(Display &&) = delete;

    void render()
    {
      renderer.render();
    }
  };
}
