#pragma once
#include <vector>
#include "ImageDescriptor.h"
#include <LLUtils/Buffer.h>
namespace IMCodec
{
	enum class ImageItemType
	{
	  	  Container
		, Image
		, ImageMaskPair
		, Mipmap
		, Pages
		, AnimationFrame
		, Unknown
	};

	struct ExifData
	{
		int orientation;
		double longitude;
		double latitude = -1;
	};

	struct ItemMetaData
	{
		ExifData exifData{};
	};

	struct ItemRuntimeData
	{
		double loadTime{};
		double displayTime{};
		std::wstring pluginUsed;
	};

	struct AnimationData
	{
		uint32_t delayMilliseconds{};
	};



	struct ImageItem
	{
		/// <summary>
		/// Image description , e.g. width and height.
		/// </summary>
		ImageDescriptor descriptor{};


		/// <summary>
		/// Image  meta data.
		/// </summary>

		ItemMetaData metaData{};
		/// <summary>
		/// Data added to the item that resolves in runtime.
		/// </summary>
		ItemRuntimeData runtimeData{};

		/// <summary>
		/// The raw image data
		/// </summary>
		LLUtils::Buffer data{};
		/// <summary>
		/// THe type of the item
		/// </summary>	
		ImageItemType itemType{};

		/// <summary>
		/// Animation data, applicable only form AnimationData item type.
		/// </summary>
		AnimationData animationData{};
	};

	using ImageItemSharedPtr = std::shared_ptr<ImageItem>;
}










