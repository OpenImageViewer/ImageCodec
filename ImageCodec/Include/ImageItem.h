#pragma once
#include <vector>
#include "ImageDescriptor.h"
#include <LLUtils/Buffer.h>
#include <limits>
#include "ImageCommon.h"

namespace IMCodec
{
	enum class ImageItemType
	{
		  Image // Default
		, Container
		, Mipmap
		, Pages
		, AnimationFrame
		, Unknown
	};

	struct ExifData
	{
		int orientation = 0;
		double longitude = std::numeric_limits<double>::max();
		double latitude = std::numeric_limits<double>::max();
		double altitude = std::numeric_limits<double>::max();
		int8_t relativeAltitude = std::numeric_limits<int8_t>::max(); // 0 = Above Sea Level, 1 = Below Sea Level
		std::string make;
		std::string model;
		std::string software;
		std::string copyright;

		union
		{
			uint16_t flashValue;
			struct _flash
			{
				unsigned char flashFired : 1;
				unsigned char returnLight : 2;
				unsigned char flashMode : 2;
				unsigned char flashPresent : 1;
				unsigned char redeyeCorretion : 1;
				unsigned char reserved1 : 1;
				unsigned char reserved2 : 8;
			} flash;
		} flash;
		

	};

	struct ItemMetaData
	{
		ExifData exifData{};
	};

	struct ItemProcessData
	{
		double processTime{}; // Process time for decode / encode in ms
		PluginID pluginUsed;
	};

	using ItemProcessDataSharedPtr = std::shared_ptr<ItemProcessData>;

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
		/// Data added to the item that resolves in runtime.
		/// </summary>
		//ItemRuntimeData runtimeData{};

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

		/// <summary>
		/// codec process information
		/// </summary>
		ItemProcessData processData{};
	};

	using ImageItemSharedPtr = std::shared_ptr<ImageItem>;
	using ItemMetaDataSharedPtr = std::shared_ptr<ItemMetaData>;
}










