#include <iostream>
#include <ImageLoader.h>
#include  <LLUtils/StopWatch.h>

int main()
{
	// In this example, cat.jpg is being loaded and converted into cat.png
	using namespace IMCodec;
	ImageSharedPtr out_image;
	IMCodec::ImageLoader loader;
	int resultCode{};
	if (loader.Decode(LLUTILS_TEXT("cat.jpg"),ImageLoadFlags::None, {},PluginTraverseMode::AnyPlugin,out_image)
		== ImageResult::Success)
	{
		std::cout << "\ncat.jpg has been decoded in: " << std::setprecision(2) << std::fixed << out_image->GetProcessData().processTime << " ms";

		LLUtils::Buffer encodedBuffer;
		LLUtils::StopWatch encodeTime(true);
		if (loader.Encode(out_image, LLUTILS_TEXT("./output/cat.png")) == ImageResult::Success)
		{
			
			std::cout << "\ncat.png has been encoded in: " << std::setprecision(2) << std::fixed<< encodeTime.GetElapsedTimeReal(LLUtils::StopWatch::Milliseconds) << " ms";
		}
	}
	else
	{
		resultCode = 1;
	}

	std::cout << "\nPress any key to continue...";
	getchar();

	return resultCode;
}