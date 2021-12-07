#ifndef _VKTIMAGESAMPLECOUNTSTESTS_HPP
#define _VKTIMAGESAMPLECOUNTSTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"
#include <functional>

namespace vkt
{
namespace image
{

tcu::TestCaseGroup*		createImageSampleCountsTests		(tcu::TestContext& testCtx);

namespace
{
using namespace vk;
struct CaseDef
{
	vk::VkFormat		format;
	vk::VkImageType		imageType;
	vk::VkImageTiling	imageTiling;
	VkImageUsageFlags	usageFlags;
	bool				singleUsageFlag;
};

class SampleCountTestInstance : public TestInstance
{
public:
	enum SampleCountsSubtests
	{
		LINEAR_TILING_AND_NOT_2D_IMAGE_TYPE,
		CUBE_COMPATIBLE_SUBTEST,
		OPTIMAL_TILING_FEATURES_SUBTEST,
		EXTERNAL_IIMAGE_TYPE_SUBTEST,
		YCBCR_CONVERSION_SUBTEST,
		USAGE_FLAGS_SUBTEST,
		ONE_SAMPLE_COUNT_PRESENT_SUBTEST
	};
public:
	SampleCountTestInstance 	(Context& context, const CaseDef& m_caseDef, SampleCountsSubtests subtest);
	tcu::TestStatus	 iterate 	(void) override;

private:
	bool checkLinearTilingAndNot2DImageType	();
	bool checkCubeCompatible				();
	bool checkOptimalTilingFeatures			();
	bool checkExternalImageType				();
	bool check_YCBCR_Conversion				();
	bool checkUsageFlags					();
	bool checkOneSampleCountPresent			();

	VkSampleCountFlags	getColorSampleCounts		(const VkPhysicalDeviceProperties2& physicalDeviceProperties,
		const vk::VkPhysicalDeviceVulkan12Properties& physicalDeviceProperties12);
	VkSampleCountFlags	getDepthStencilSampleCounts	(const VkPhysicalDeviceProperties2& physicalDeviceProperties);
	VkSampleCountFlags	getSampledSampleCounts		(const VkPhysicalDeviceProperties2& physicalDeviceProperties);
	VkSampleCountFlags	getStorageSampleCounts		(const VkPhysicalDeviceProperties2& physicalDeviceProperties);

private:
	CaseDef					m_caseDef;
	SampleCountsSubtests	m_subtest;
};

class SampleCountTest : public TestCase
{
public:
	SampleCountTest (tcu::TestContext& testCtx, const std::string& name,
		const std::string& description, const CaseDef& caseDef,
		SampleCountTestInstance::SampleCountsSubtests subtest);
	TestInstance* 	createInstance 	(Context& context) const override;
	void 			checkSupport 	(Context& context) const override;

private:
	CaseDef											m_caseDef;
	SampleCountTestInstance::SampleCountsSubtests	m_subtest;
};

} // anonymous

} // image
} // vkt

#endif // _VKTIMAGESAMPLECOUNTSTESTS_HPP
