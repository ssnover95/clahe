/*
 * file: utility.cpp
 */

#include "utility.hpp"
#include <opencv2/opencv.hpp>

namespace snover
{
int generateGrayscaleHistogram(cv::Mat const & image, IMAGE_HISTOGRAM & outputHistogram)
{
    if (outputHistogram.histogram->size() != 256)
    {
        return -1;
    }

    for (auto rowIdx = 0u; rowIdx < image.rows; ++rowIdx)
    {
        for (auto colIdx = 0u; colIdx < image.cols; ++colIdx)
        {
            (*(outputHistogram.histogram))[image.at<uint8_t>(rowIdx, colIdx)]++;
        }
    }

    return 0;
}

int createHistogramPlot(IMAGE_HISTOGRAM const & histogram,
                        unsigned int width,
                        unsigned int height,
                        cv::Mat & outputImage)
{
    unsigned int const numberOfBins(256);
    unsigned int const binWidth(width / numberOfBins);
    float const verticalScaleFactor(static_cast<float>(histogram.max()) / height);

    outputImage = cv::Mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    for (auto i = 1u; i < numberOfBins; ++i)
    {
        cv::line(outputImage,
                 cv::Point(binWidth * (i - 1),
                           static_cast<int>(height - histogram[i - 1] / verticalScaleFactor)), // point1
                 cv::Point(binWidth * i,
                           static_cast<int>(height - histogram[i] / verticalScaleFactor)), // point 2
                 cv::Scalar(255, 255, 255)); // line color
    }

    return 0;
}

int createCDFPlot(IMAGE_HISTOGRAM const & histogram,
                  unsigned int width,
                  unsigned int height,
                  cv::Mat & outputImage)
{
    unsigned int const numberOfBins(256);
    unsigned int const elementWidth(width / numberOfBins);
    float const verticalScaleFactor(static_cast<float>([&histogram]() {
                                        auto numberOfPixels = 0;
                                        for (auto i = 0u; i < 256; ++i)
                                        {
                                            numberOfPixels += histogram[i];
                                        }
                                        return numberOfPixels;
                                    }()) /
                                    height);

    outputImage = cv::Mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    float cumulativeSum = histogram[0];
    for (auto i = 1u; i < numberOfBins; ++i)
    {
        cv::line(outputImage,
                 cv::Point(elementWidth * (i - 1),
                           static_cast<int>(height - cumulativeSum / verticalScaleFactor)),
                 cv::Point(elementWidth * i,
                           static_cast<int>(height - (cumulativeSum + histogram[i]) / verticalScaleFactor)),
                 cv::Scalar(255, 255, 255));
    }

    return 0;
}

float calculateEntropy(cv::Mat const & image)
{
    IMAGE_HISTOGRAM temp;
    generateGrayscaleHistogram(image, temp);

    auto totalPixels(image.rows * image.cols);
    float entropy = 0.0;

    for (auto i = 0u; i < 256; ++i)
    {
        float proportion = static_cast<float>(temp[i]) / totalPixels;
        entropy += -1 * proportion * log2(proportion);
    }

    return isnan(entropy) ? 0.f : entropy;
}

int getSubregionOfImage(cv::Mat const & input, cv::Rect const & region, cv::Mat & output)
{
    if (region.x + region.width > input.cols || region.y + region.height > input.rows)
    {
        return -1;
    }

    input(region).copyTo(output);
    return 0;
}

GRAY_LEVEL classifyGrayLevel(IMAGE_HISTOGRAM const & histogram)
{
    unsigned long numberOfPixels = [&histogram]() {
        unsigned long total = 0;
        for (auto & iter : *(histogram.histogram))
        {
            total += iter;
        }
        return total;
    }();

    unsigned int cumulativeSum[] = {0, 0, 0};

    for (auto i = 0u; i <= 255 / 3; ++i)
    {
        cumulativeSum[0] += histogram[i];
    }
    for (auto i = 255 / 3; i <= 255 / 3 * 2; ++i)
    {
        cumulativeSum[1] += histogram[i];
    }
    for (auto i = 255 / 3 * 2; i <= 255; ++i)
    {
        cumulativeSum[2] += histogram[i];
    }

    uint8_t maxLevel = 0;
    if (cumulativeSum[1] > cumulativeSum[0])
    {
        maxLevel = 1;
    }
    if (cumulativeSum[2] > cumulativeSum[maxLevel])
    {
        maxLevel = 2;
    }

    return static_cast<GRAY_LEVEL>(maxLevel);
}

PIXEL bilinearInterpolate(std::vector<PIXEL> & pixels, float outX, float outY)
{
    if (pixels.size() != 4)
    {
        abort();
    }

    PIXEL retVal{static_cast<unsigned int>(outX), static_cast<unsigned int>(outY), 0};

    // Sort the four pixels into the order of top left, bottom left, top right, bottom right
    std::sort(pixels.begin(), pixels.end());

    float x0 = pixels[0].x;
    float y0 = pixels[0].y;
    float x1 = pixels[3].x;
    float y1 = pixels[3].y;

    // Bilinear interpolation function
    retVal.intensity = static_cast<unsigned int>(((y1 - outY) / (y1 - y0)) *
                           ((x1 - outX) / (x1 - x0) * pixels[0].intensity +
                            (outX - x0) / (x1 - x0) * pixels[2].intensity) +
                       ((outY - y0) / (y1 - y0)) *
                           ((x1 - outX) / (x1 - x0) * pixels[1].intensity +
                            (outX - x0) / (x1 - x0) * pixels[3].intensity));
    return retVal;
}

PIXEL linearInterpolate(PIXEL pixel0, PIXEL pixel1, float outX, float outY)
{
    float x0 = pixel0.x;
    float x1 = pixel1.x;
    // Linear interpolation of the pixel's grayscale intensity
    auto finalIntensity = static_cast<unsigned int>(pixel0.intensity + (static_cast<float>(pixel1.intensity) - pixel0.intensity) * ((outX - x0) / (x1 - x0)));
    return {static_cast<unsigned int>(outX), static_cast<unsigned int>(outY), finalIntensity};
}

void clipHistogram(IMAGE_HISTOGRAM & histogram, double clipLimit)
{
    unsigned int numberOfPixelsOverLimit(0);

    // Clip each bin quantity and count how many were excess of the clip limit
    for (auto i = 0u; i < 256; ++i)
    {
        if (histogram[i] > clipLimit)
        {
            numberOfPixelsOverLimit += histogram[i] - clipLimit;
            histogram.histogram->operator[](i) = static_cast<unsigned int>(clipLimit);
        }
    }

    // Iterate over the bins and add a pixel to the quantity of the bins until
    // no more pixels are left.
    // Using a uint8_t to take advantage of overflow.
    uint8_t binIndex(0);
    while (numberOfPixelsOverLimit > 0)
    {
        histogram.histogram->operator[](binIndex++) = numberOfPixelsOverLimit--;
    }
}

} // namespace snover