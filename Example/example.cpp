#include <ImageLoader.h>
#include <LLUtils/FileHelper.h>
#include <ImageCodec.h>

int main()
{
	// In this example, cat.jpg is being loaded and converted into cat.png
	using namespace IMCodec;
	ImageCodec codec;
	ImageSharedPtr out_image;
	IMCodec::ImageLoader loader(&codec, nullptr);
	auto inputFileBuffer = LLUtils::File::ReadAllBytes(L"cat.jpg");
	if (codec.Decode(inputFileBuffer.data(), inputFileBuffer.size(), {}, ImageLoadFlags::None, {}, {}, out_image)
		== ImageResult::Success)
	{
		LLUtils::Buffer encodedBuffer;
		auto pluginID = loader.GetFirstPlugin(L"png");
		if (loader.Encode(out_image, L"./output/cat.png") == ImageResult::Success)
			return 0;
	}
	return 1;
}