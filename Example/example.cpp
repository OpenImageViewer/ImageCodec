#include <ImageLoader.h>
#include <LLUtils/FileHelper.h>

int main()
{
	// In this example, cat.jpg is being loaded and converted into cat.png
	using namespace IMCodec;
	ImageCodec codec;
	ImageSharedPtr out_image;
	auto inputFileBuffer = LLUtils::File::ReadAllBytes(L"cat.jpg");
	if (codec.Decode(inputFileBuffer.data(), inputFileBuffer.size(), {}, ImageLoadFlags::None, ImageLoaderFlags::OnlyRegisteredExtensionRelaxed, {}, out_image)
		== ImageResult::Success)
	{
		LLUtils::Buffer encodedBuffer;
		if (codec.Encode(out_image, L"png", encodedBuffer) == ImageResult::Success)
		{
			LLUtils::File::WriteAllBytes(L"./output/cat.png", encodedBuffer.size(), encodedBuffer.data());
			return 0;
		}
	}
	return 1;
}