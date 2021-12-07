/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Image sampleCounts tests which are according to description [34.1.1. Supported Sample Counts]
 * of [Vulkan® 1.2.203 - A Specification]
 *//*--------------------------------------------------------------------*/

#include "vktImageSampleCountsTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vktImageLoadStoreUtil.hpp"
#include "vktImageTexture.hpp"

#include "vkDefs.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deUniquePtr.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace image
{

namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using tcu::IVec3;

bool SampleCountTestInstance::checkExternalImageType ()
{
	VkExternalMemoryHandleTypeFlagBits handleTypes[] =
	{
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT
	};

	int size = sizeof(handleTypes) / sizeof(handleTypes[0]);

	for	(int i = 0; i < size; i++)
	{
		VkPhysicalDeviceExternalImageFormatInfo externalInfo =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
			NULL,
			handleTypes[i]
		};

		VkPhysicalDeviceImageFormatInfo2 info =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
			&externalInfo,
			m_caseDef.format,
			m_caseDef.imageType,
			m_caseDef.imageTiling,
			0,
			0
		};

		VkExternalMemoryProperties externalMemoryProperties =
		{
			VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT,
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
		};

		VkExternalImageFormatProperties externalProperties =
		{
			VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
			NULL,
			externalMemoryProperties
		};

		VkImageFormatProperties2 properties =
		{
			VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
			&externalProperties,
			VkImageFormatProperties()
		};

		auto res = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(
			m_context.getPhysicalDevice(), &info, &properties);

		if (res == VK_SUCCESS && properties.imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
		{
			return false;
		}
	}

	return true;
}

// Return true if a is superset of b
bool isSuperset (VkSampleCountFlags a, VkSampleCountFlags b)
{
	return (a & b) == b;
}

VkSampleCountFlags
SampleCountTestInstance::getColorSampleCounts (const VkPhysicalDeviceProperties2& physicalDeviceProperties,
	const vk::VkPhysicalDeviceVulkan12Properties& physicalDeviceProperties12)
{
	if (!isCompressedFormat(m_caseDef.format))
	{
		// If usage includes VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and format is a floating- or fixed-point color format,
		// a superset of VkPhysicalDeviceLimits::framebufferColorSampleCounts
		if (vk::isFloatFormat(m_caseDef.format) || isSnormFormat(m_caseDef.format) || isUnormFormat(m_caseDef.format))
		{
			return physicalDeviceProperties.properties.limits.framebufferColorSampleCounts;
		}
		// If usage includes VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and format is an integer format,
		// a superset of VkPhysicalDeviceVulkan12Properties::framebufferIntegerColorSampleCounts
		else if (vk::isIntFormat(m_caseDef.format) || vk::isUintFormat(m_caseDef.format))
		{
			return physicalDeviceProperties12.framebufferIntegerColorSampleCounts;
		}
	}

	return 0;
}

VkSampleCountFlags
SampleCountTestInstance::getDepthStencilSampleCounts (const VkPhysicalDeviceProperties2& physicalDeviceProperties)
{
	if (!isCompressedFormat(m_caseDef.format))
	{
		auto format = mapVkFormat(m_caseDef.format);
		// If usage includes VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, and format includes a depth aspect,
		// a superset of VkPhysicalDeviceLimits::framebufferDepthSampleCounts
		if (format.order == tcu::TextureFormat::D)
		{
			return physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts;
		}
		// If usage includes VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, and format includes a stencil aspect,
		// a superset of VkPhysicalDeviceLimits::framebufferStencilSampleCounts
		else if (format.order == tcu::TextureFormat::S)
		{
			return physicalDeviceProperties.properties.limits.framebufferStencilSampleCounts;
		}
	}

	return 0;
}

VkSampleCountFlags
SampleCountTestInstance::getSampledSampleCounts (const VkPhysicalDeviceProperties2& physicalDeviceProperties)
{
	if (!isCompressedFormat(m_caseDef.format) && !isYCbCrFormat(m_caseDef.format))
	{
		auto format = mapVkFormat(m_caseDef.format);

		// If usage includes VK_IMAGE_USAGE_SAMPLED_BIT, and format includes a color aspect,
		// a superset of VkPhysicalDeviceLimits::sampledImageColorSampleCounts
		if (format.order != tcu::TextureFormat::D &&
		   	format.order != tcu::TextureFormat::DS &&
		   	format.order != tcu::TextureFormat::S)
		{
			return physicalDeviceProperties.properties.limits.sampledImageColorSampleCounts;
		}
		// If usage includes VK_IMAGE_USAGE_SAMPLED_BIT, and format includes a depth aspect,
		// a superset of VkPhysicalDeviceLimits::sampledImageDepthSampleCounts
		else if (format.order == tcu::TextureFormat::D || format.order == tcu::TextureFormat::DS)
		{
			return physicalDeviceProperties.properties.limits.sampledImageDepthSampleCounts;
		}

		// If usage includes VK_IMAGE_USAGE_SAMPLED_BIT, and format is an integer format,
		// a superset of VkPhysicalDeviceLimits::sampledImageIntegerSampleCounts
		if (vk::isIntFormat(m_caseDef.format) || vk::isUintFormat(m_caseDef.format))
		{
			return physicalDeviceProperties.properties.limits.sampledImageIntegerSampleCounts;
		}
	}

	return 0;
}

VkSampleCountFlags
SampleCountTestInstance::getStorageSampleCounts (const VkPhysicalDeviceProperties2& physicalDeviceProperties)
{
	return physicalDeviceProperties.properties.limits.storageImageSampleCounts;
}

void SampleCountTest::checkSupport (Context& ctx) const
{
	VkImageFormatProperties imageFormatProperties;
	VkResult imageFormatResult = ctx.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		ctx.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling,
		m_caseDef.usageFlags, 0, &imageFormatProperties);

	if (imageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
	{
		TCU_THROW(NotSupportedError, "Format is not supported");
	}
}

SampleCountTestInstance::SampleCountTestInstance (
	Context& context,
	const CaseDef& caseDef,
	SampleCountsSubtests subtest) :
		TestInstance(context), m_caseDef(caseDef), m_subtest(subtest)
{
}

bool SampleCountTestInstance::checkUsageFlags ()
{
	vk::VkPhysicalDeviceVulkan12Properties 	physicalDeviceProperties12;
	physicalDeviceProperties12.sType 		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
	physicalDeviceProperties12.pNext 		= 0;

	vk::VkPhysicalDeviceProperties2 		physicalDeviceProperties;
	physicalDeviceProperties.sType 			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties.pNext 			= &physicalDeviceProperties12;

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &physicalDeviceProperties);

	VkImageFormatProperties 				imageFormatProperties;
	VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType,
		m_caseDef.imageTiling, m_caseDef.usageFlags, 0, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	if (m_caseDef.singleUsageFlag)
	{
		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			return isSuperset(imageFormatProperties.sampleCounts, getColorSampleCounts(physicalDeviceProperties, physicalDeviceProperties12));
		}

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			return isSuperset(imageFormatProperties.sampleCounts, getDepthStencilSampleCounts(physicalDeviceProperties));
		}

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			return isSuperset(imageFormatProperties.sampleCounts, getSampledSampleCounts(physicalDeviceProperties));
		}

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		{
			return isSuperset(imageFormatProperties.sampleCounts, getStorageSampleCounts(physicalDeviceProperties));
		}
	}
	// If multiple bits are set in usage, sampleCounts will be the intersection of the per-usage values described above.
	else
	{
		VkSampleCountFlags sampleCountFlags = 0;

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			sampleCountFlags |= getColorSampleCounts(physicalDeviceProperties, physicalDeviceProperties12);
		}

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			sampleCountFlags |= getDepthStencilSampleCounts(physicalDeviceProperties);
		}

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			sampleCountFlags |= getSampledSampleCounts(physicalDeviceProperties);
		}

		if (m_caseDef.usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		{
			sampleCountFlags |= getStorageSampleCounts(physicalDeviceProperties);
		}

		return imageFormatProperties.sampleCounts & sampleCountFlags;
	}

	return false;
}

bool SampleCountTestInstance::check_YCBCR_Conversion ()
{
	VkImageFormatProperties imageFormatProperties;
	VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType,
		m_caseDef.imageTiling, 0, 0, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS || imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
	{
		return false;
	}

	return true;
}

bool SampleCountTestInstance::checkLinearTilingAndNot2DImageType ()
{
	VkImageFormatProperties imageFormatProperties;
	const VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType,
		m_caseDef.imageTiling, 0, 0, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	if (imageFormatProperties.sampleCounts == VK_SAMPLE_COUNT_1_BIT &&
		(m_caseDef.imageTiling == VK_IMAGE_TILING_LINEAR || 		// tiling is VK_IMAGE_TILING_LINEAR
		m_caseDef.imageType != VK_IMAGE_TYPE_2D))				// type is not VK_IMAGE_TYPE_2D
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool SampleCountTestInstance::checkCubeCompatible ()
{
	VkImageFormatProperties imageFormatProperties;
	VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType,
		m_caseDef.imageTiling, 0, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	// flags contains VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	if (imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
	{
		return false;
	}

	return true;
}

bool SampleCountTestInstance::checkOptimalTilingFeatures ()
{
	vk::VkFormatProperties formatProperties;
	m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), m_caseDef.format, &formatProperties);

	VkImageFormatProperties imageFormatProperties;
	VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling,
		0, 0, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	// Neither the VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT flag nor
	// the VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT flag in
	// VkFormatProperties::optimalTilingFeatures returned by vkGetPhysicalDeviceFormatProperties is set
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) &&
		!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
	   	imageFormatProperties.sampleCounts != VK_SAMPLE_COUNT_1_BIT)
	{
		return false;
	}

	return true;
}

bool SampleCountTestInstance::checkOneSampleCountPresent ()
{
	VkImageFormatProperties imageFormatProperties;
	VkResult imageFormatResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
		m_context.getPhysicalDevice(), m_caseDef.format, m_caseDef.imageType, m_caseDef.imageTiling,
		0, 0, &imageFormatProperties);

	if (imageFormatResult != VK_SUCCESS)
	{
		return false;
	}

	// If none of the bits described above are set in usage, then there is no corresponding limit in
	// VkPhysicalDeviceLimits. In this case, sampleCounts must include at least VK_SAMPLE_COUNT_1_BIT.
	if (!(imageFormatProperties.sampleCounts & VK_SAMPLE_COUNT_1_BIT))
	{
		return false;
	}

	return true;
}

tcu::TestStatus	SampleCountTestInstance::iterate ()
{
	bool result = false;

	switch (m_subtest)
	{
		case LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE:
		{
			result = checkLinearTilingAndNot2DImageType();
			break;
		}
		case CUBE_COMPATIBLE_SUBTEST:
		{
			result = checkCubeCompatible();
			break;
		}
		case OPTIMAL_TILING_FEATURES_SUBTEST:
		{
			result = checkOptimalTilingFeatures();
			break;
		}
		case EXTERNAL_IIMAGE_TYPE_SUBTEST:
		{
			result = checkExternalImageType();
			break;
		}
		case YCBCR_CONVERSION_SUBTEST:
		{
			result = check_YCBCR_Conversion();
			break;
		}
		case USAGE_FLAGS_SUBTEST:
		{
			result = checkUsageFlags();
			break;
		}
		case ONE_SAMPLE_COUNT_PRESENT_SUBTEST:
		{
			result = checkOneSampleCountPresent();
			break;
		}
	}

	return result ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("FAILED");
}

SampleCountTest::SampleCountTest(tcu::TestContext& testCtx, const std::string& name,
	const std::string& description, const CaseDef& caseDef,
	SampleCountTestInstance::SampleCountsSubtests subtest) :
		TestCase(testCtx, name, description), m_caseDef(caseDef), m_subtest(subtest)
{
}

vkt::TestInstance* SampleCountTest::createInstance (Context& context) const
{
	return new SampleCountTestInstance(context, m_caseDef, m_subtest);
}

} // anonymous ns

void addUsageFlagsSubtests (tcu::TestContext& testCtx, const std::string& samplesCaseName, const CaseDef& caseDef,
	tcu::TestCaseGroup *group)
{
	const VkImageUsageFlagBits usageFlags[]  =
	{
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_USAGE_STORAGE_BIT
	};

	unsigned sizeUsageFlags	= sizeof(usageFlags) / sizeof(usageFlags[0]);
	unsigned sizeUsageSet	= 1 << sizeUsageFlags;

	for (unsigned i = 1; i < sizeUsageSet; i++)
	{
		VkImageUsageFlags usage = 0;
		for (unsigned j = 0; j < sizeUsageFlags; j++)
		{
			if ((i >> j) & 1)
			{
				usage |= usageFlags[j];
			}
		}

		std::string caseName = samplesCaseName + "_USAGE_FLAGS";
		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			caseName += "_COLOR";
		}

		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			caseName += "_DEPTHSTENCIL";
		}

		if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			caseName += "_SAMPLED";
		}

		if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
		{
			caseName += "_STORAGE";
		}

		caseName += "_SUBTEST";

		bool singleUsageFlag = i==1 || i==2 || i==4 || i==8 ? true : false;

		CaseDef newCaseDef =
		{
			caseDef.format,
			caseDef.imageType,
			caseDef.imageTiling,
			usage,
			singleUsageFlag
		};

		group->addChild(new SampleCountTest(testCtx, caseName, "", newCaseDef, SampleCountTestInstance::USAGE_FLAGS_SUBTEST));
	}
}

tcu::TestCaseGroup* createImageSampleCountsTests (tcu::TestContext& testCtx)
{
	const VkImageTiling imageTilings[] =
	{
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_TILING_LINEAR,
	};

	const VkImageType imageTypes[] =
	{
		VK_IMAGE_TYPE_1D,
		VK_IMAGE_TYPE_2D,
		VK_IMAGE_TYPE_3D
	};

	const VkFormat formats[] =
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_UINT,
		VK_FORMAT_R64G64B64A64_SINT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		VK_FORMAT_BC2_UNORM_BLOCK,
		VK_FORMAT_BC2_SRGB_BLOCK,
		VK_FORMAT_BC3_UNORM_BLOCK,
		VK_FORMAT_BC3_SRGB_BLOCK,
		VK_FORMAT_BC4_UNORM_BLOCK,
		VK_FORMAT_BC4_SNORM_BLOCK,
		VK_FORMAT_BC5_UNORM_BLOCK,
		VK_FORMAT_BC5_SNORM_BLOCK,
		VK_FORMAT_BC6H_UFLOAT_BLOCK,
		VK_FORMAT_BC6H_SFLOAT_BLOCK,
		VK_FORMAT_BC7_UNORM_BLOCK,
		VK_FORMAT_BC7_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
		VK_FORMAT_G8B8G8R8_422_UNORM,
		VK_FORMAT_B8G8R8G8_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		VK_FORMAT_R10X6_UNORM_PACK16,
		VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_R12X4_UNORM_PACK16,
		VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
		VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G16B16G16R16_422_UNORM,
		VK_FORMAT_B16G16R16G16_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
		VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
		VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,
		VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT,
		VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT,
		VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT,
		VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT,
		VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
		VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT
	};

	MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "sample_counts", "Image sample counts"));

	for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(imageTypes); typeNdx++)
	{
		std::string imageTypeName;
		switch (imageTypes[typeNdx])
		{
			case VK_IMAGE_TYPE_1D:
			{
				imageTypeName = getImageTypeName(IMAGE_TYPE_1D);
				break;
			}

			case VK_IMAGE_TYPE_2D:
			{
				imageTypeName = getImageTypeName(IMAGE_TYPE_2D);
				break;
			}

			case VK_IMAGE_TYPE_3D:
			{
				imageTypeName = getImageTypeName(IMAGE_TYPE_3D);
				break;
			}

			default:
				break;
		};

		MovePtr<tcu::TestCaseGroup>	imageViewGroup		(new tcu::TestCaseGroup(testCtx, imageTypeName.c_str(), ""));
		for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY( imageTilings); tilingNdx++)
		{
			const std::string			tilingGroupName	= getImageTilingName(imageTilings[tilingNdx]);
			MovePtr<tcu::TestCaseGroup>	tilingGroup		(new tcu::TestCaseGroup(testCtx, tilingGroupName.c_str(), ""));

			for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
			{

				const std::string samplesCaseName = "imageFormat_" + getFormatShortString(formats[formatNdx]);

				const CaseDef caseDef =
				{
					formats[formatNdx],
					imageTypes[typeNdx],
					imageTilings[tilingNdx],
					0,
					false
				};

				if (caseDef.imageType == VK_IMAGE_TYPE_2D && caseDef.imageTiling == VK_IMAGE_TILING_OPTIMAL)
				{
					tilingGroup.get()->addChild(new SampleCountTest(testCtx,
						samplesCaseName + "_CUBE_COMPATIBLE_SUBTEST", "", caseDef,
						SampleCountTestInstance::CUBE_COMPATIBLE_SUBTEST));

					tilingGroup.get()->addChild(new SampleCountTest(testCtx,
						samplesCaseName + "_OPTIMAL_TILING_FEATURES_SUBTEST", "", caseDef,
						SampleCountTestInstance::OPTIMAL_TILING_FEATURES_SUBTEST));

					if (isYCbCrFormat(caseDef.format))
					{
						tilingGroup.get()->addChild(new SampleCountTest(testCtx,
						samplesCaseName + "_EXTERNAL_IIMAGE_TYPE_SUBTEST", "", caseDef,
						SampleCountTestInstance::EXTERNAL_IIMAGE_TYPE_SUBTEST));
					}

					tilingGroup.get()->addChild(new SampleCountTest(testCtx,
						samplesCaseName + "_YCBCR_CONVERSION_SUBTEST", "", caseDef,
						SampleCountTestInstance::YCBCR_CONVERSION_SUBTEST));

					addUsageFlagsSubtests(testCtx, samplesCaseName, caseDef, tilingGroup.get());

					tilingGroup.get()->addChild(new SampleCountTest(testCtx,
						samplesCaseName + "_ONE_SAMPLE_COUNT_PRESENT_SUBTEST", "", caseDef,
						SampleCountTestInstance::ONE_SAMPLE_COUNT_PRESENT_SUBTEST));
				}
				else
				{
					tilingGroup.get()->addChild(new SampleCountTest(testCtx,
						samplesCaseName + "LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE_SUBTEST", "", caseDef,
						SampleCountTestInstance::LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE));
				}
			}
			imageViewGroup->addChild(tilingGroup.release());
		}
		testGroup->addChild(imageViewGroup.release());
	}

	return testGroup.release();
}

} // image

} // vkt
