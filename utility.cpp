/*
 * file: utility.cpp
 */

#include <cassert>
#include "utility.hpp"
#include <opencv2/opencv.hpp>

int generateGrayscaleHistogram(cv::Mat const & image, ImageHistogram & outputHistogram)
{
    if (outputHistogram.histogram.size() != 256)
    {
        return -1;
    }

    for (auto rowIdx = 0u; rowIdx < image.rows; ++rowIdx)
    {
        for (auto colIdx = 0u; colIdx < image.cols; ++colIdx)
        {
            (outputHistogram.histogram)[image.at<uint8_t>(rowIdx, colIdx)]++;
        }
    }

    return 0;
}

ImageHistogram generateGrayscaleHistogramForSubregion(cv::Mat const & image, Rectangle const & region)
{
    assert(region.height + region.y <= image.rows);
    assert(region.width + region.x <= image.cols);
    ImageHistogram output{};

    for (auto rowIdx = region.y; rowIdx < (region.height + region.y); ++rowIdx)
    {
        for (auto colIdx = region.x; colIdx < (region.width + region.x); ++colIdx)
        {
            output.histogram[image.at<uint8_t>(rowIdx, colIdx)]++;
        }
    }

    return output;
}

GrayLevel classifyGrayLevel(ImageHistogram const & histogram)
{
    unsigned long const numberOfPixels = [&histogram]() {
        unsigned long total = 0;
        for (auto & iter : histogram.histogram)
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

    return static_cast<GrayLevel>(maxLevel);
}

Pixel bilinearInterpolate(std::vector<Pixel> & pixels, float outX, float outY)
{
    if (pixels.size() != 4)
    {
        abort();
    }

    Pixel retVal{static_cast<unsigned int>(outX), static_cast<unsigned int>(outY), 0};

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

Pixel linearInterpolate(Pixel pixel0, Pixel pixel1, float outX, float outY)
{
    if (pixel1.y == pixel0.y)
    {
        float x0 = pixel0.x;
        float x1 = pixel1.x;
        // Linear interpolation of the pixel's grayscale intensity
        auto finalIntensity = static_cast<unsigned int>(pixel0.intensity +
                                                        (static_cast<float>(pixel1.intensity) - pixel0.intensity) *
                                                        ((outX - x0) / (x1 - x0)));
        return {static_cast<unsigned int>(outX), static_cast<unsigned int>(outY), finalIntensity};
    }
    else if (pixel1.x == pixel0.x)
    {
        float y0 = pixel0.y;
        float y1 = pixel1.y;

        auto finalIntensity = static_cast<unsigned int>(pixel0.intensity +
                                                        (static_cast<float>(pixel1.intensity) - pixel0.intensity) *
                                                        ((outY - y0) / (y1 - y0)));
        return {static_cast<unsigned int>(outX), static_cast<unsigned int>(outY), finalIntensity};
    }
    // Default case, should never occur in this program
    return {0, 0, 0};
}

void clipHistogram(ImageHistogram & histogram, double clipLimit)
{
    unsigned int numberOfPixelsOverLimit(0);

    // Clip each bin quantity and count how many were excess of the clip limit
    for (auto binIndex = 0u; binIndex < 256; ++binIndex)
    {
        if (histogram[binIndex] > clipLimit)
        {
            numberOfPixelsOverLimit += histogram[binIndex] - clipLimit;
            histogram.histogram[binIndex] = static_cast<unsigned int>(clipLimit);
        }
    }

    unsigned int excessPixelsPerBin(numberOfPixelsOverLimit / 256);

    for (auto binIndex = 0u; binIndex < 256; ++binIndex)
    {
        histogram.histogram[binIndex] += excessPixelsPerBin;
    }
}
