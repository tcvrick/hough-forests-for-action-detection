#include "HoughForests.h"
#include "LocalFeatureExtractor.h"
#include "STIPFeature.h"
#include "Trainer.h"
#include "Utils.h"

#include <omp.h>

#include <numpy.hpp>

#include <boost/format.hpp>

#include <Eigen/Core>

#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

void extractPositiveFeatures(const std::string& videoDirectoryPath,
                             const std::string& outputDirectoryPath, int localWidth,
                             int localHeight, int localDuration, int xBlockSize, int yBlockSize,
                             int tBlockSize, int xStep, int yStep, int tStep, int nSamplesPerStep) {
    using namespace nuisken::houghforests;

    std::vector<double> scales = {1.0};
    int randomSeed = 1;
    std::mt19937 randomEngine(randomSeed);

    std::tr2::sys::path directory(videoDirectoryPath);
    std::tr2::sys::directory_iterator end;
    for (std::tr2::sys::directory_iterator itr(directory); itr != end; ++itr) {
        std::string filePath = itr->path().string();

        std::cout << "extract" << std::endl;
        LocalFeatureExtractor extractor(filePath, scales, localWidth, localHeight, localDuration,
                                        xBlockSize, yBlockSize, tBlockSize, xStep, yStep, tStep);
        std::vector<cv::Vec3i> selectedPoints;
        std::vector<std::vector<float>> selectedDescriptors;
        while (true) {
            std::cout << "frame: " << extractor.getStoredFeatureBeginT() << std::endl;
            std::vector<std::vector<cv::Vec3i>> points;
            std::vector<std::vector<std::vector<float>>> descriptors;
            extractor.extractLocalFeatures(points, descriptors);
            if (extractor.isEnded()) {
                break;
            }

            std::vector<size_t> indices(points[0].size());
            std::iota(std::begin(indices), std::end(indices), 0);
            std::shuffle(std::begin(indices), std::end(indices), randomEngine);

            int n = 0;
            for (auto index : indices) {
                if (n++ >= nSamplesPerStep) {
                    break;
                }

                selectedPoints.push_back(points[0][index]);
                selectedDescriptors.push_back(descriptors[0][index]);
            }
        }

        std::cout << "output: " << selectedPoints.size() << std::endl;
        std::string outputFileName = itr->path().filename().stem().string();
        std::string outputPointsFilePath = outputDirectoryPath + outputFileName + "_pt.npy";
        std::vector<int> outputPoints;
        for (const auto& point : selectedPoints) {
            for (int i = 0; i < point.rows; ++i) {
                outputPoints.push_back(point(i));
            }
        }
        aoba::SaveArrayAsNumpy<int>(outputPointsFilePath, selectedPoints.size(),
                                    selectedPoints.front().rows, outputPoints.data());

        std::string outputDescriptorsFilePath = outputDirectoryPath + outputFileName + "_desc.npy";
        std::vector<float> outputDescriptors;
        for (const auto& desc : selectedDescriptors) {
            for (int i = 0; i < desc.size(); ++i) {
                outputDescriptors.push_back(desc[i]);
            }
        }
        aoba::SaveArrayAsNumpy<float>(outputDescriptorsFilePath, selectedDescriptors.size(),
                                      selectedDescriptors.front().size(), outputDescriptors.data());
    }
}

void readLabelsInfo(const std::string& labelFilePath, int sequenceIndex,
                    std::vector<int>& classLabels, std::vector<cv::Rect>& boxes,
                    std::vector<std::pair<int, int>>& temporalRanges) {
    std::ifstream inputStream(labelFilePath);
    std::string line;
    while (std::getline(inputStream, line)) {
        boost::char_separator<char> commaSeparator(",");
        boost::tokenizer<boost::char_separator<char>> commaTokenizer(line, commaSeparator);
        std::vector<std::string> tokens;
        std::copy(std::begin(commaTokenizer), std::end(commaTokenizer), std::back_inserter(tokens));

        if (tokens.at(0) == "seq" + std::to_string(sequenceIndex)) {
            auto label = std::stoi(tokens.at(1));
            classLabels.push_back(label);

            auto beginFrame = std::stoi(tokens.at(2));
            auto endFrame = std::stoi(tokens.at(3));
            auto topLeftX = std::stoi(tokens.at(4));
            auto topLeftY = std::stoi(tokens.at(5));
            auto bottomRightX = std::stoi(tokens.at(6));
            auto bottomRightY = std::stoi(tokens.at(7));
            boxes.emplace_back(cv::Point(topLeftX, topLeftY),
                               cv::Point(bottomRightX, bottomRightY));
            temporalRanges.emplace_back(beginFrame, endFrame);
        }
    }
}

void readLabelsInfo(const std::string& labelFilePath, int sequenceIndex,
                    std::vector<cv::Rect>& boxes,
                    std::vector<std::pair<int, int>>& temporalRanges) {
    std::ifstream inputStream(labelFilePath);
    std::string line;
    while (std::getline(inputStream, line)) {
        boost::char_separator<char> commaSeparator(",");
        boost::tokenizer<boost::char_separator<char>> commaTokenizer(line, commaSeparator);
        std::vector<std::string> tokens;
        std::copy(std::begin(commaTokenizer), std::end(commaTokenizer), std::back_inserter(tokens));

        if (tokens.at(0) == "seq" + std::to_string(sequenceIndex)) {
            auto beginFrame = std::stoi(tokens.at(2));
            auto endFrame = std::stoi(tokens.at(3));
            auto topLeftX = std::stoi(tokens.at(4));
            auto topLeftY = std::stoi(tokens.at(5));
            auto bottomRightX = std::stoi(tokens.at(6));
            auto bottomRightY = std::stoi(tokens.at(7));

            boxes.emplace_back(cv::Point(topLeftX, topLeftY),
                               cv::Point(bottomRightX, bottomRightY));
            temporalRanges.emplace_back(beginFrame, endFrame);
        }
    }
}

bool contains(const cv::Rect& box, const std::pair<int, int>& temporalRange,
              const cv::Vec3i& point) {
    bool space = box.contains(cv::Point(point(2), point(1)));
    bool time = (temporalRange.first <= point(0)) && (temporalRange.second < point(0));
    return space && time;
}

bool contains(const std::vector<cv::Rect>& boxes,
              const std::vector<std::pair<int, int>>& temporalRanges, const cv::Vec3i& point) {
    for (int i = 0; i < boxes.size(); ++i) {
        if (contains(boxes.at(i), temporalRanges.at(i), point)) {
            return true;
        }
    }
    return false;
}

void extractNegativeFeatures(const std::string& videoDirectoryPath,
                             const std::string& labelFilePath,
                             const std::string& outputDirectoryPath, int localWidth,
                             int localHeight, int localDuration, int xBlockSize, int yBlockSize,
                             int tBlockSize, int xStep, int yStep, int tStep,
                             const std::vector<double>& scales, int nSamplesPerStep,
                             int beginSequenceIndex, int endSequenceIndex) {
    using namespace nuisken::houghforests;

    int randomSeed = 1;
    std::mt19937 randomEngine(randomSeed);

    for (int sequenceIndex = beginSequenceIndex; sequenceIndex <= endSequenceIndex;
         ++sequenceIndex) {
        std::string filePath =
                (boost::format("%sseq%d.avi") % videoDirectoryPath % sequenceIndex).str();
        std::vector<cv::Rect> boxes;
        std::vector<std::pair<int, int>> temporalRanges;
        std::cout << "read labels" << std::endl;
        readLabelsInfo(labelFilePath, sequenceIndex, boxes, temporalRanges);

        std::cout << "extract" << std::endl;
        LocalFeatureExtractor extractor(filePath, scales, localWidth, localHeight, localDuration,
                                        xBlockSize, yBlockSize, tBlockSize, xStep, yStep, tStep);
        std::vector<cv::Vec3i> selectedPoints;
        std::vector<std::vector<float>> selectedDescriptors;
        while (true) {
            std::cout << "frame: " << extractor.getStoredFeatureBeginT() << std::endl;
            std::vector<std::vector<cv::Vec3i>> points;
            std::vector<std::vector<std::vector<float>>> descriptors;
            extractor.extractLocalFeatures(points, descriptors);
            if (extractor.isEnded()) {
                break;
            }

            std::cout << "select" << std::endl;
            for (int scaleIndex = 0; scaleIndex < points.size(); ++scaleIndex) {
                std::vector<size_t> indices;
                int index = 0;
                for (const auto& point : points[scaleIndex]) {
                    cv::Vec3i scaledPoint(point);
                    scaledPoint(1) /= scales[scaleIndex];
                    scaledPoint(2) /= scales[scaleIndex];
                    if (!contains(boxes, temporalRanges, point)) {
                        indices.push_back(index);
                    }
                    ++index;
                }

                std::shuffle(std::begin(indices), std::end(indices), randomEngine);

                int n = 0;
                for (auto index : indices) {
                    if (n++ >= nSamplesPerStep) {
                        break;
                    }

                    selectedPoints.push_back(points[scaleIndex][index]);
                    selectedDescriptors.push_back(descriptors[scaleIndex][index]);
                }
            }
        }

        std::cout << "output: " << selectedPoints.size() << std::endl;
        std::string outputFileName = (boost::format("seq%d") % sequenceIndex).str();
        std::string outputPointsFilePath = outputDirectoryPath + outputFileName + "_pt.npy";
        std::vector<int> outputPoints;
        for (const auto& point : selectedPoints) {
            for (int i = 0; i < point.rows; ++i) {
                outputPoints.push_back(point(i));
            }
        }
        aoba::SaveArrayAsNumpy<int>(outputPointsFilePath, selectedPoints.size(),
                                    selectedPoints.front().rows, outputPoints.data());

        std::string outputDescriptorsFilePath = outputDirectoryPath + outputFileName + "_desc.npy";
        std::vector<float> outputDescriptors;
        for (const auto& desc : selectedDescriptors) {
            for (int i = 0; i < desc.size(); ++i) {
                outputDescriptors.push_back(desc[i]);
            }
        }
        aoba::SaveArrayAsNumpy<float>(outputDescriptorsFilePath, selectedDescriptors.size(),
                                      selectedDescriptors.front().size(), outputDescriptors.data());
    }
}

void train(const std::string& featureDirectoryPath, const std::string& labelFilePath,
           const std::string& forestsDirectoryPath, int baseScale, int nTrees,
           double bootstrapRatio, int maxDepth, int minData, int nSplits, int nThresholds,
           int beginValidationIndex, int endValidationIndex) {
    using namespace nuisken::storage;
    using namespace nuisken::houghforests;
    using namespace nuisken::randomforests;

    const int N_CHANNELS = 4;
    const int N_CLASSES = 7;

    std::vector<std::vector<int>> validationCombinations = {{19, 5}, {12, 6}, {4, 7},   {16, 17},
                                                            {9, 13}, {11, 8}, {10, 14}, {18, 15},
                                                            {3, 20}, {2, 1}};

    for (int validationIndex = beginValidationIndex; validationIndex < endValidationIndex;
         ++validationIndex) {
        std::cout << "validation: " << validationIndex << std::endl;
        std::vector<std::shared_ptr<STIPFeature>> trainingData;
        for (int i = 0; i < validationCombinations.size(); ++i) {
            if (i == validationIndex) {
                continue;
            }
            for (int sequenceIndex : validationCombinations.at(i)) {
                std::cout << "read seq" << sequenceIndex << std::endl;
                std::vector<int> classLabels;
                std::vector<cv::Rect> boxes;
                std::vector<std::pair<int, int>> ranges;
                readLabelsInfo(labelFilePath, sequenceIndex, classLabels, boxes, ranges);
                for (int labelIndex = 0; labelIndex < classLabels.size(); ++labelIndex) {
                    std::string baseFilePath =
                            (boost::format("%sseq%d_%d_%d") % featureDirectoryPath % sequenceIndex %
                             labelIndex % classLabels[labelIndex])
                                    .str();
                    std::string pointFilePath = baseFilePath + "_pt.npy";
                    std::vector<int> pointShape;
                    std::vector<int> points;
                    aoba::LoadArrayFromNumpy<int>(pointFilePath, pointShape, points);

                    std::string descriptorFilePath = baseFilePath + "_desc.npy";
                    std::vector<int> descShape;
                    std::vector<float> descriptors;
                    aoba::LoadArrayFromNumpy<float>(descriptorFilePath, descShape, descriptors);

                    int nChannelFeatures = descShape[1] / N_CHANNELS;
                    for (int localIndex = 0; localIndex < pointShape[0]; ++localIndex) {
                        int pointIndex = localIndex * 3;
                        cv::Vec3i point(points[pointIndex], points[pointIndex + 1],
                                        points[pointIndex + 2]);
                        std::vector<Eigen::MatrixXf> features(N_CHANNELS);
                        for (int channelIndex = 0; channelIndex < N_CHANNELS; ++channelIndex) {
                            Eigen::MatrixXf feature(1, nChannelFeatures);
                            for (int featureIndex = 0; featureIndex < nChannelFeatures;
                                 ++featureIndex) {
                                int index = localIndex * descShape[1] +
                                            channelIndex * nChannelFeatures + featureIndex;
                                feature.coeffRef(0, featureIndex) = descriptors[index];
                            }
                            features.at(channelIndex) = feature;
                        }
                        cv::Vec3i actionPosition;
                        actionPosition(0) =
                                (ranges[labelIndex].second - ranges[labelIndex].first) / 2;
                        double aspectRatio = static_cast<double>(boxes[labelIndex].width) /
                                             boxes[labelIndex].height;
                        actionPosition(1) = baseScale / 2;
                        actionPosition(2) = baseScale * aspectRatio / 2;
                        // std::cout << actionPosition << std::endl;
                        cv::Vec3i offset = actionPosition - point;
                        auto data = std::make_shared<STIPFeature>(features, point, offset,
                                                                  std::make_pair(0.0, 0.0),
                                                                  classLabels[labelIndex]);
                        data->setIndex(-1);
                        trainingData.push_back(data);
                    }
                }

                std::string baseFilePath =
                        (boost::format("%sseq%d") % featureDirectoryPath % sequenceIndex).str();
                std::string pointFilePath = baseFilePath + "_pt.npy";
                std::vector<int> pointShape;
                std::vector<int> points;
                aoba::LoadArrayFromNumpy<int>(pointFilePath, pointShape, points);

                std::string descriptorFilePath = baseFilePath + "_desc.npy";
                std::vector<int> descShape;
                std::vector<float> descriptors;
                aoba::LoadArrayFromNumpy<float>(descriptorFilePath, descShape, descriptors);

                int nChannelFeatures = descShape[0] / N_CHANNELS;

                for (int localIndex = 0; localIndex < pointShape[1]; ++localIndex) {
                    int pointIndex = localIndex * 3;
                    cv::Vec3i point(points[pointIndex], points[pointIndex + 1],
                                    points[pointIndex + 2]);
                    std::vector<Eigen::MatrixXf> features(N_CHANNELS);
                    for (int channelIndex = 0; channelIndex < N_CHANNELS; ++channelIndex) {
                        Eigen::MatrixXf feature(1, nChannelFeatures);
                        for (int featureIndex = 0; featureIndex < nChannelFeatures;
                             ++featureIndex) {
                            int index = localIndex * descShape[0] +
                                        channelIndex * nChannelFeatures + featureIndex;
                            feature.coeffRef(0, featureIndex) = descriptors[index];
                        }
                        features.at(channelIndex) = feature;
                    }
                    auto data = std::make_shared<STIPFeature>(
                            features, point, cv::Vec3i(), std::make_pair(0.0, 0.0), N_CLASSES - 1);
                    data->setIndex(-1);
                    trainingData.push_back(data);
                }
            }
        }
        std::cout << "data size: " << trainingData.size() << std::endl;

        auto type = TreeParameters::ALL_RATIO;
        bool hasNegatieClass = true;
        TreeParameters treeParameters(N_CLASSES, nTrees, bootstrapRatio, maxDepth, minData, nSplits,
                                      nThresholds, type, hasNegatieClass);
        std::vector<int> numberOfFeatureDimensions(N_CHANNELS);
        for (auto i = 0; i < N_CHANNELS; ++i) {
            numberOfFeatureDimensions.at(i) = trainingData.front()->getNumberOfFeatureDimensions(i);
        }
        STIPNode stipNode(N_CLASSES, N_CHANNELS, numberOfFeatureDimensions);
        HoughForestsParameters houghParameters;
        houghParameters.setTreeParameters(treeParameters);
        int nThreads = 6;
        HoughForests houghForests(stipNode, houghParameters, nThreads);
        houghForests.train(trainingData);

        std::string outputDirectoryPath =
                (boost::format("%s%d/") % forestsDirectoryPath % validationIndex).str();
        std::tr2::sys::path directory(outputDirectoryPath);
        if (!std::tr2::sys::exists(directory)) {
            std::tr2::sys::create_directory(directory);
        }

        houghForests.save(outputDirectoryPath);
    }
}

void detect(const std::string& forestsDirectoryPath, const std::string& videoFilePath,
            int localWidth, int localHeight, int localDuration, int xBlockSize, int yBlockSize,
            int tBlockSize, int xStep, int yStep, int tStep, const std::vector<double>& scales,
            int nThreads, int width, int height, int baseScale, const std::vector<int>& binSizes,
            int votesDeleteStep, int votesBufferLength, const std::vector<double>& scoreThresholds,
            double iouThreshold) {
    using namespace nuisken;
    using namespace nuisken::houghforests;
    using namespace nuisken::randomforests;
    using namespace nuisken::storage;

    LocalFeatureExtractor extractor(scales, localWidth, localHeight, localDuration, xBlockSize,
                                    yBlockSize, tBlockSize, xStep, yStep, tStep);
    cv::VideoCapture capture(videoFilePath);

    int nClasses = 7;
    std::vector<double> bandwidths = {10.0, 8.0, 0.5};
    std::vector<int> steps = {binSizes.at(1), binSizes.at(0)};
    double votingSpaceDiscretizeRatio = 0.5;
    int invalidLeafSizeThreshold = 300;
    std::vector<double> aspectRatios = {1.23, 1.22, 1.42, 0.69, 1.46, 1.72};
    std::vector<std::size_t> durations = {100, 116, 66, 83, 62, 85};
    bool hasNegativeClass = true;
    bool isBackprojection = false;
    TreeParameters treeParameters(nClasses, 0, 0, 0, 0, 0, 0, TreeParameters::ALL_RATIO,
                                  hasNegativeClass);
    HoughForestsParameters parameters(
            width, height, scales, baseScale, nClasses, bandwidths.at(0), bandwidths.at(1),
            bandwidths.at(2), steps.at(0), steps.at(1), binSizes, votesDeleteStep,
            votesBufferLength, invalidLeafSizeThreshold, scoreThresholds, durations, aspectRatios,
            iouThreshold, hasNegativeClass, isBackprojection, treeParameters);
    HoughForests houghForests(nThreads);
    houghForests.setHoughForestsParameters(parameters);
    houghForests.load(forestsDirectoryPath);

    int fps = 30;
    std::vector<std::vector<DetectionResult<4>>> detectionResults;
    houghForests.detect(extractor, capture, fps, detectionResults, true, cv::Size(width, height),
                        std::vector<cv::Vec3i>(nClasses, cv::Vec3i(255, 0, 0)));
}

std::vector<std::size_t> readDurations(const std::string& filePath) {
    std::ifstream inputStream(filePath);
    std::string line;

    std::vector<std::size_t> durations;
    while (std::getline(inputStream, line)) {
        boost::char_separator<char> commaSeparator(",");
        boost::tokenizer<boost::char_separator<char>> commaTokenizer(line, commaSeparator);
        std::vector<std::string> tokens;
        std::copy(std::begin(commaTokenizer), std::end(commaTokenizer), std::back_inserter(tokens));

        durations.push_back(std::stoi(tokens.at(1)));
    }
    return durations;
}

std::vector<double> readAspectRatios(const std::string& filePath) {
    std::ifstream inputStream(filePath);
    std::string line;

    std::vector<double> aspectRatios;
    while (std::getline(inputStream, line)) {
        boost::char_separator<char> commaSeparator(",");
        boost::tokenizer<boost::char_separator<char>> commaTokenizer(line, commaSeparator);
        std::vector<std::string> tokens;
        std::copy(std::begin(commaTokenizer), std::end(commaTokenizer), std::back_inserter(tokens));

        aspectRatios.push_back(std::stod(tokens.at(1)));
    }
    return aspectRatios;
}

void detectAll(const std::string& forestsDirectoryPath, const std::string& outputDirectoryPath,
               const std::string& videoDirectoryPath, const std::string& durationDirectoryPath,
               const std::string& aspectDirectoryPath, int localWidth, int localHeight,
               int localDuration, int xBlockSize, int yBlockSize, int tBlockSize, int xStep,
               int yStep, int tStep, const std::vector<double>& scales, int nThreads, int width,
               int height, int baseScale, const std::vector<int>& binSizes, int votesDeleteStep,
               int votesBufferLength, const std::vector<double>& scoreThresholds,
               double iouThreshold, int beginValidationIndex, int endValidationIndex) {
    using namespace nuisken;
    using namespace nuisken::houghforests;
    using namespace nuisken::randomforests;
    using namespace nuisken::storage;

    int nClasses = 7;
    std::vector<double> bandwidths = {10.0, 8.0, 0.5};
    std::vector<int> steps = {binSizes.at(1), binSizes.at(0)};
    double votingSpaceDiscretizeRatio = 0.5;
    int invalidLeafSizeThreshold = 300;
    bool hasNegativeClass = true;
    bool isBackprojection = false;
    TreeParameters treeParameters(nClasses, 0, 0, 0, 0, 0, 0, TreeParameters::ALL_RATIO,
                                  hasNegativeClass);

    std::vector<std::vector<int>> validationCombinations = {{19, 5}, {12, 6}, {4, 7},   {16, 17},
                                                            {9, 13}, {11, 8}, {10, 14}, {18, 15},
                                                            {3, 20}, {2, 1}};

    for (int validationIndex = beginValidationIndex; validationIndex < endValidationIndex;
         ++validationIndex) {
        std::vector<double> aspectRatios =
                readAspectRatios(aspectDirectoryPath + std::to_string(validationIndex) + ".csv");
        std::vector<std::size_t> durations =
                readDurations(durationDirectoryPath + std::to_string(validationIndex) + ".csv");
        HoughForestsParameters parameters(
                width, height, scales, baseScale, nClasses, bandwidths.at(0), bandwidths.at(1),
                bandwidths.at(2), steps.at(0), steps.at(1), binSizes, votesDeleteStep,
                votesBufferLength, invalidLeafSizeThreshold, scoreThresholds, durations,
                aspectRatios, iouThreshold, hasNegativeClass, isBackprojection, treeParameters);

        std::cout << "validation: " << validationIndex << std::endl;
        HoughForests houghForests(nThreads);
        houghForests.setHoughForestsParameters(parameters);
        std::string forestsDir = forestsDirectoryPath + std::to_string(validationIndex) + "/";
        houghForests.load(forestsDir);
        for (int sequenceIndex : validationCombinations.at(validationIndex)) {
            std::string videoFilePath =
                    (boost::format("%sseq%d.avi") % videoDirectoryPath % sequenceIndex).str();
            LocalFeatureExtractor extractor(scales, localWidth, localHeight, localDuration,
                                            xBlockSize, yBlockSize, tBlockSize, xStep, yStep,
                                            tStep);
            cv::VideoCapture capture(videoFilePath);

            std::vector<std::vector<DetectionResult<4>>> detectionResults;
            houghForests.detect(extractor, capture, 40, detectionResults);

            std::cout << "output" << std::endl;
            for (auto classLabel = 0; classLabel < detectionResults.size(); ++classLabel) {
                std::string outputFilePath = (boost::format("%s%d_%d_detection.txt") %
                                              outputDirectoryPath % sequenceIndex % classLabel)
                                                     .str();

                std::ofstream outputStream(outputFilePath);
                for (const auto& detectionResult : detectionResults.at(classLabel)) {
                    LocalMaximum localMaximum = detectionResult.getLocalMaximum();
                    outputStream << "LocalMaximum," << localMaximum.getPoint()(T) << ","
                                 << localMaximum.getPoint()(Y) << "," << localMaximum.getPoint()(X)
                                 << "," << localMaximum.getValue() << ","
                                 << localMaximum.getPoint()(3) << std::endl;

                    auto contributionPoints = detectionResult.getContributionPoints();
                    for (const auto& contributionPoint : contributionPoints) {
                        outputStream << contributionPoint.getPoint()(T) << ","
                                     << contributionPoint.getPoint()(Y) << ","
                                     << contributionPoint.getPoint()(X) << ","
                                     << contributionPoint.getValue() << std::endl;
                    }
                    outputStream << std::endl;
                }
            }
        }
    }
}

void detectWebCamera(const std::string& forestsDirectoryPath,
                     const std::vector<double>& aspectRatios,
                     const std::vector<std::size_t>& durations, int localWidth, int localHeight,
                     int localDuration, int xBlockSize, int yBlockSize, int tBlockSize, int xStep,
                     int yStep, int tStep, const std::vector<double>& scales, int nClasses,
                     int nThreads, int width, int height, int baseScale,
                     const std::vector<int>& binSizes, int votesDeleteStep, int votesBufferLength,
                     int invalidLeafSizeThreshold, const std::vector<double>& scoreThresholds,
                     double iouThreshold, int fps,
                     const std::vector<cv::Vec3i>& visualizationColors) {
    using namespace nuisken;
    using namespace nuisken::houghforests;
    using namespace nuisken::randomforests;
    using namespace nuisken::storage;

    LocalFeatureExtractor extractor(scales, localWidth, localHeight, localDuration, xBlockSize,
                                    yBlockSize, tBlockSize, xStep, yStep, tStep);
    cv::VideoCapture capture(0);
    capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    std::vector<double> bandwidths = {10.0, 8.0, 0.5};
    std::vector<int> steps = {binSizes.at(1), binSizes.at(0)};
    bool hasNegativeClass = true;
    bool isBackprojection = false;
    TreeParameters treeParameters(nClasses, 0, 0, 0, 0, 0, 0, TreeParameters::ALL_RATIO,
                                  hasNegativeClass);
    HoughForestsParameters parameters(
            width, height, scales, baseScale, nClasses, bandwidths.at(0), bandwidths.at(1),
            bandwidths.at(2), steps.at(0), steps.at(1), binSizes, votesDeleteStep,
            votesBufferLength, invalidLeafSizeThreshold, scoreThresholds, durations, aspectRatios,
            iouThreshold, hasNegativeClass, isBackprojection, treeParameters);
    HoughForests houghForests(nThreads);
    houghForests.setHoughForestsParameters(parameters);
    houghForests.load(forestsDirectoryPath);

    std::vector<std::vector<DetectionResult<4>>> detectionResults;
    houghForests.detect(extractor, capture, fps, detectionResults, true,
                        cv::Size(width * 3, height * 3), visualizationColors);
}

void extractMIRU2016(const std::string& positiveVideoDirectoryPath,
                     const std::string& negativeVideoDirectoryPath,
                     const std::string& labelFilePath, const std::string& dstDirectoryPath,
                     int localWidth, int localHeight, int localDuration, int xBlockSize,
                     int yBlockSize, int tBlockSize, int xStep, int yStep, int tStep,
                     const std::vector<double>& negativeScales, int nPositiveSamplesPerStep,
                     int nNegativeSamplesPerStep) {
    using namespace nuisken;
    Trainer trainer;
    trainer.extractTrainingFeatures(
            positiveVideoDirectoryPath, negativeVideoDirectoryPath, labelFilePath, dstDirectoryPath,
            localWidth, localHeight, localDuration, xBlockSize, yBlockSize, tBlockSize, xStep,
            yStep, tStep, negativeScales, nPositiveSamplesPerStep, nNegativeSamplesPerStep);
}

void trainMIRU2016(const std::string& featureDirectoryPath, const std::string& labelFilePath,
                   const std::string& forestsDirectoryPath,
                   const std::vector<std::vector<int>> trainingDataIndices, int nClasses,
                   int baseScale, int nTrees, double bootstrapRatio, int maxDepth, int minData,
                   int nSplits, int nThresholds, bool isMaskUsed) {
    using namespace nuisken;
    Trainer trainer;
    for (int i = 0; i < trainingDataIndices.size(); ++i) {
        std::string currentForestsDirectoryPath =
                (boost::format("%s%d/") % forestsDirectoryPath % i).str();
        trainer.train(featureDirectoryPath, labelFilePath, currentForestsDirectoryPath,
                      trainingDataIndices.at(i), nClasses, baseScale, nTrees, bootstrapRatio,
                      maxDepth, minData, nSplits, nThresholds, isMaskUsed);
    }
}

void detectMIRU2016CV(const std::string& forestsDirectoryPath,
                      const std::string& outputDirectoryPath, const std::string& videoDirectoryPath,
                      const std::string& durationDirectoryPath,
                      const std::string& aspectDirectoryPath, int localWidth, int localHeight,
                      int localDuration, int xBlockSize, int yBlockSize, int tBlockSize, int xStep,
                      int yStep, int tStep, const std::vector<double>& scales, int nThreads,
                      int width, int height, int baseScale, const std::vector<int>& binSizes,
                      int votesDeleteStep, int votesBufferLength,
                      const std::vector<double>& scoreThresholds, double iouThreshold,
                      int beginValidationIndex, int endValidationIndex) {
    using namespace nuisken;
    using namespace nuisken::houghforests;
    using namespace nuisken::randomforests;
    using namespace nuisken::storage;

    int nClasses = 7;
    std::vector<double> bandwidths = {10.0, 8.0, 0.5};
    std::vector<int> steps = {binSizes.at(1), binSizes.at(0)};
    double votingSpaceDiscretizeRatio = 0.5;
    int invalidLeafSizeThreshold = 200;
    bool hasNegativeClass = true;
    bool isBackprojection = false;
    TreeParameters treeParameters(nClasses, 0, 0, 0, 0, 0, 0, TreeParameters::ALL_RATIO,
                                  hasNegativeClass);

    std::vector<std::vector<int>> validationCombinations(10);
    for (int i = 0; i < validationCombinations.size(); ++i) {
        for (int j = 0; j < 2; ++j) {
            validationCombinations.at(i).push_back(i * 2 + j);
        }
    }

    for (int validationIndex = beginValidationIndex; validationIndex < endValidationIndex;
         ++validationIndex) {
        std::vector<double> aspectRatios =
                readAspectRatios(aspectDirectoryPath + std::to_string(validationIndex) + ".csv");
        std::vector<std::size_t> durations =
                readDurations(durationDirectoryPath + std::to_string(validationIndex) + ".csv");
        HoughForestsParameters parameters(
                width, height, scales, baseScale, nClasses, bandwidths.at(0), bandwidths.at(1),
                bandwidths.at(2), steps.at(0), steps.at(1), binSizes, votesDeleteStep,
                votesBufferLength, invalidLeafSizeThreshold, scoreThresholds, durations,
                aspectRatios, iouThreshold, hasNegativeClass, isBackprojection, treeParameters);

        std::cout << "validation: " << validationIndex << std::endl;
        HoughForests houghForests(nThreads);
        houghForests.setHoughForestsParameters(parameters);
        std::string forestsDir = forestsDirectoryPath + std::to_string(validationIndex) + "/";
        houghForests.load(forestsDir);
        for (int sequenceIndex : validationCombinations.at(validationIndex)) {
            std::string videoFilePath =
                    (boost::format("%s%d.avi") % videoDirectoryPath % sequenceIndex).str();
            LocalFeatureExtractor extractor(scales, localWidth, localHeight, localDuration,
                                            xBlockSize, yBlockSize, tBlockSize, xStep, yStep,
                                            tStep);
            cv::VideoCapture capture(videoFilePath);

            std::vector<std::vector<DetectionResult<4>>> detectionResults;
            houghForests.detect(extractor, capture, 40, detectionResults);

            std::cout << "output" << std::endl;
            for (auto classLabel = 0; classLabel < detectionResults.size(); ++classLabel) {
                std::string outputFilePath = (boost::format("%s%d_%d_detection.txt") %
                                              outputDirectoryPath % sequenceIndex % classLabel)
                                                     .str();
                std::ofstream outputStream(outputFilePath);
                for (const auto& detectionResult : detectionResults.at(classLabel)) {
                    LocalMaximum localMaximum = detectionResult.getLocalMaximum();
                    outputStream << "LocalMaximum," << localMaximum.getPoint()(T) << ","
                                 << localMaximum.getPoint()(Y) << "," << localMaximum.getPoint()(X)
                                 << "," << localMaximum.getValue() << ","
                                 << localMaximum.getPoint()(3) << std::endl;

                    auto contributionPoints = detectionResult.getContributionPoints();
                    for (const auto& contributionPoint : contributionPoints) {
                        outputStream << contributionPoint.getPoint()(T) << ","
                                     << contributionPoint.getPoint()(Y) << ","
                                     << contributionPoint.getPoint()(X) << ","
                                     << contributionPoint.getValue() << std::endl;
                    }
                    outputStream << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    const cv::String keys = "{m mode||mode}";
    cv::CommandLineParser parser(argc, argv, keys);
    int mode = parser.get<int>("m");

    //{
    //	const cv::String keys =
    //		"{p posvid||positive video dir}"
    //		"{n negvid||negative video dir}"
    //		"{f feat||dst feat dir}"
    //		"{w lw||local width}"
    //		"{h lh||local height}"
    //		"{d ld||local duration}"
    //		"{x xb||x block size}"
    //		"{y yb||y block size}"
    //		"{t tb||t block size}"
    //		"{a xs||x step size}"
    //		"{b ys||y step size}"
    //		"{c ts||y step size}";
    //	"{s sb||base scale}";

    //	cv::CommandLineParser parser(argc, argv, keys);

    //	//std::string rootDirectoryPath = "D:/miru2016/";
    //	std::string rootDirectoryPath = "F:/Hara/miru2016/";
    //	std::string positiveVideoDirectoryPath = rootDirectoryPath + "segmented_300/";
    //	std::string negativeVideoDirectoryPath = rootDirectoryPath + "unsegmented/";
    //	std::string labelFilePath = rootDirectoryPath + "labels.csv";
    //	std::string dstDirectoryPath = rootDirectoryPath + "feature_300/";
    //	int localWidth = 21;
    //	int localHeight = localWidth;
    //	int localDuration = 9;
    //	int xBlockSize = 7;
    //	int yBlockSize = xBlockSize;
    //	int tBlockSize = 3;
    //	int xStep = 11;
    //	int yStep = xStep;
    //	int tStep = 5;
    //	std::vector<double> negativeScales = {1.0, 0.707, 0.5};
    //	int nPositiveSamplesPerStep = 10;
    //	int nNegativeSamplesPerStep = 10;
    //	int baseScale = 300;
    //	//extractMIRU2016(positiveVideoDirectoryPath, negativeVideoDirectoryPath, labelFilePath,
    //	//                dstDirectoryPath, localWidth, localHeight, localDuration, xBlockSize,
    //	//                yBlockSize, tBlockSize, xStep, yStep, tStep, negativeScales,
    //	//                nPositiveSamplesPerStep, nNegativeSamplesPerStep);
    //}

    if (mode == 0) {
        const cv::String keys =
                "{p posvid||positive video dir}"
                "{n negvid||negative video dir}"
                "{f feat||dst feat dir}"
                "{w lw||local width}"
                "{d ld||local duration}"
                "{x xb||x block size}"
                "{t tb||t block size}"
                "{a xs||x step size}"
                "{c ts||y step size}"
                "{s sb||base scale}";
        cv::CommandLineParser parser(argc, argv, keys);

        // std::string rootDirectoryPath = "D:/miru2016/";
        std::string rootDirectoryPath = "F:/Hara/miru2016/";
        std::string positiveVideoDirectoryPath = rootDirectoryPath + parser.get<std::string>("p");
        std::string negativeVideoDirectoryPath = rootDirectoryPath + parser.get<std::string>("n");
        std::string labelFilePath = rootDirectoryPath + "labels.csv";
        std::string dstDirectoryPath = rootDirectoryPath + parser.get<std::string>("f");
        ;
        int localWidth = parser.get<int>("w");
        int localHeight = localWidth;
        int localDuration = parser.get<int>("d");
        int xBlockSize = parser.get<int>("x");
        int yBlockSize = xBlockSize;
        int tBlockSize = parser.get<int>("t");
        int xStep = parser.get<int>("a");
        int yStep = xStep;
        int tStep = parser.get<int>("c");
        std::vector<double> negativeScales = {1.0, 0.707, 0.5};
        int nPositiveSamplesPerStep = 10;
        int nNegativeSamplesPerStep = 10;
        int baseScale = parser.get<int>("s");
        extractMIRU2016(positiveVideoDirectoryPath, negativeVideoDirectoryPath, labelFilePath,
                        dstDirectoryPath, localWidth, localHeight, localDuration, xBlockSize,
                        yBlockSize, tBlockSize, xStep, yStep, tStep, negativeScales,
                        nPositiveSamplesPerStep, nNegativeSamplesPerStep);
    }

    //  {
    ////std::string rootDirectoryPath = "D:/miru2016/";
    // std::string rootDirectoryPath = "F:/Hara/miru2016/";
    //      std::string featureDirectoryPath = rootDirectoryPath + "feature/";
    //      std::string labelFilePath = rootDirectoryPath + "labels.csv";
    //      std::string forestsDirectoryPath = rootDirectoryPath + "forests/";
    //      std::vector<std::vector<int>> trainingDataIndices(1);
    // trainingDataIndices.at(0).resize(20);
    // std::iota(std::begin(trainingDataIndices.at(0)), std::end(trainingDataIndices.at(0)), 0);
    //      int nClasses = 7;
    //      int baseScale = 150;
    //      int nTrees = 15;
    //      double bootstrapRatio = 1.0;
    //      int maxDepth = 25;
    //      int minData = 10;
    //      int nSplits = 30;
    //      int nThresholds = 10;
    //      //trainMIRU2016(featureDirectoryPath, labelFilePath, forestsDirectoryPath,
    //      //              trainingDataIndices, nClasses, baseScale, nTrees, bootstrapRatio,
    //      maxDepth,
    //      //              minData, nSplits, nThresholds);
    //  }

    if (mode == 1) {
        const cv::String keys =
                "{f feat||feat dir}"
                "{d dst||dst forests dir}"
                "{t nt||ntrees}"
                "{s sb||base scale}"
                "{b bm||bool mask used}";
        cv::CommandLineParser parser(argc, argv, keys);

        // std::string rootDirectoryPath = "D:/miru2016/";
        std::string rootDirectoryPath = "F:/Hara/miru2016/";
        std::string featureDirectoryPath = rootDirectoryPath + parser.get<std::string>("f");
        std::string labelFilePath = rootDirectoryPath + "labels.csv";
        std::string forestsDirectoryPath = rootDirectoryPath + parser.get<std::string>("d");
        std::vector<std::vector<int>> trainingDataIndices(10);
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 20; ++j) {
                if (j != (i * 2) && j != (i * 2 + 1)) {
                    trainingDataIndices.at(i).push_back(j);
                }
            }
        }
        int nClasses = 7;
        int baseScale = parser.get<int>("s");
        int nTrees = parser.get<int>("t");
        double bootstrapRatio = 1.0;
        int maxDepth = 25;
        int minData = 10;
        int nSplits = 30;
        int nThresholds = 10;
        bool isMaskUsed = parser.get<bool>("b");
        trainMIRU2016(featureDirectoryPath, labelFilePath, forestsDirectoryPath,
                      trainingDataIndices, nClasses, baseScale, nTrees, bootstrapRatio, maxDepth,
                      minData, nSplits, nThresholds, isMaskUsed);
    }

    {
        // std::string rootDirectoryPath = "D:/miru2016/";
        std::string rootDirectoryPath = "F:/Hara/miru2016/";
        std::string featureDirectoryPath = rootDirectoryPath + "feature_300/";
        std::string labelFilePath = rootDirectoryPath + "labels.csv";
        std::string forestsDirectoryPath = rootDirectoryPath + "forests_300_t30/";
        std::vector<std::vector<int>> trainingDataIndices(10);
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 20; ++j) {
                if (j != (i * 2) && j != (i * 2 + 1)) {
                    trainingDataIndices.at(i).push_back(j);
                }
            }
        }
        int nClasses = 7;
        int baseScale = 300;
        int nTrees = 30;
        double bootstrapRatio = 1.0;
        int maxDepth = 25;
        int minData = 10;
        int nSplits = 30;
        int nThresholds = 10;
        // trainMIRU2016(featureDirectoryPath, labelFilePath, forestsDirectoryPath,
        //			  trainingDataIndices, nClasses, baseScale, nTrees, bootstrapRatio,
        // maxDepth,
        //			  minData, nSplits, nThresholds);
    }

    if (mode == 3) {
        std::string rootDirectoryPath = "F:/Hara/miru2016/";
        // std::string rootDirectoryPath = "E:/Hara/miru2016/";
        const cv::String keys =
                "{s scoreth||score threshold}"
                "{f fps||fps}"
                "{w width||width}"
                "{h height||height}"
                "{l leaf||leaf size threshold}";
        cv::CommandLineParser parser(argc, argv, keys);

        int localWidth = 21;
        int localHeight = localWidth;
        int localDuration = 9;
        int xBlockSize = 7;
        int yBlockSize = xBlockSize;
        int tBlockSize = 3;
        int xStep = 11;
        int yStep = xStep;
        int tStep = 5;
        std::vector<double> scales = {1.0};
        int baseScale = 300;

        std::string forestPath = rootDirectoryPath + "forests_300/0/";
        std::vector<double> aspectRatios = {0.559, 0.811, 0.570, 0.623, 0.932, 0.994};
        std::vector<std::size_t> durations = {67, 66, 70, 68, 83, 66};
        int nClasses = 7;
        int nThreads = 6;
        int width = parser.get<int>("w");
        int height = parser.get<int>("h");
        std::vector<int> binSizes = {10, 20, 20};
        int votesDeleteStep = 50;
        int votesBufferLength = 200;
        int invalidLeafSizeThreshold = parser.get<int>("l");
        std::vector<double> scoreThresholds(nClasses - 1, parser.get<double>("s"));
        double iouThreshold = 0.3;
        int fps = parser.get<int>("f");
        //  detectWebCamera(forestPath, aspectRatios, durations, localWidth, localHeight,
        // localDuration, xBlockSize, yBlockSize, tBlockSize,
        // xStep, yStep, tStep, scales, nClasses,
        // nThreads, width, height, baseScale, binSizes, votesDeleteStep,
        // votesBufferLength, invalidLeafSizeThreshold, scoreThresholds,
        //                  iouThreshold, fps);
    }

    if (mode == 2) {
        const cv::String keys =
                "{f forests||forests dir}"
                "{o output||output dir}"
                "{v vid||video dir}"
                "{w lw||local width}"
                "{d ld||local duration}"
                "{x xb||x block size}"
                "{t tb||t block size}"
                "{a xs||x step size}"
                "{c ts||y step size}"
                "{s sb||base scale}";
        cv::CommandLineParser parser(argc, argv, keys);

        // std::string rootDirectoryPath = "D:/miru2016/";
        std::string rootDirectoryPath = "F:/Hara/miru2016/";
        int localWidth = parser.get<int>("w");
        int localHeight = localWidth;
        int localDuration = parser.get<int>("d");
        int xBlockSize = parser.get<int>("x");
        int yBlockSize = xBlockSize;
        int tBlockSize = parser.get<int>("t");
        int xStep = parser.get<int>("a");
        int yStep = xStep;
        int tStep = parser.get<int>("c");
        std::vector<double> scales = {1.0};
        int baseScale = parser.get<int>("s");

        std::string forestPath = rootDirectoryPath + parser.get<std::string>("f");
        std::string aspectPath = rootDirectoryPath + "average_aspect_ratios/";
        std::string durationPath = rootDirectoryPath + "average_durations/";
        std::string outputPath = rootDirectoryPath + parser.get<std::string>("o");
        std::string videoPath = rootDirectoryPath + parser.get<std::string>("v");
        int nClasses = 7;
        int nThreads = 6;
        std::vector<int> binSizes = {10, 20, 20};
        int votesDeleteStep = 50;
        int votesBufferLength = 200;
        std::vector<double> scores(nClasses - 1, 0.1);
        double iouThreshold = 0.3;
        detectMIRU2016CV(forestPath, outputPath, videoPath, durationPath, aspectPath, localWidth,
                         localHeight, localDuration, xBlockSize, yBlockSize, tBlockSize, xStep,
                         yStep, tStep, scales, nThreads, 640, 360, baseScale, binSizes,
                         votesDeleteStep, votesBufferLength, scores, iouThreshold, 0, 10);
    }

    // std::string rootDirectoryPath = "D:/UT-Interaction/";
    //   std::string rootDirectoryPath = "E:/Hara/UT-Interaction/";
    //   std::string segmentedVideoDirectoryPath = rootDirectoryPath + "segmented_fixed_scale_100/";
    //   std::string videoDirectoryPath = rootDirectoryPath + "unsegmented_half/";
    //   std::string featureDirectoryPath = rootDirectoryPath + "feature_hf_pooling_half2/";
    //   std::string forestsDirectoryPath =
    //           rootDirectoryPath + "data_hf/forests_hf_pooling_half_feature2_integral/";
    //   std::string votingDirectoryPath = rootDirectoryPath + "data_hf/voting_feature3/";
    //   std::string durationDirectoryPath = rootDirectoryPath + "average_durations/";
    //   std::string aspectDirectoryPath = rootDirectoryPath + "average_aspect_ratios/";
    //   std::string labelFilePath = rootDirectoryPath + "labels.csv";

    //   int localWidth = 15;
    //   int localHeight = localWidth;
    //   int localDuration = 9;
    //   int xBlockSize = 5;
    //   int yBlockSize = 5;
    //   int tBlockSize = 3;
    //   int xStep = 8;
    //   int yStep = xStep;
    //   int tStep = 5;
    //   std::vector<double> scales = {1.0};

    //   int nPositiveSamplesPerStep = 30;
    //   int nNegativeSamplesPerStep = 3;

    //   int baseScale = 100;
    //   int nTrees = 15;
    //   double bootstrapRatio = 1.0;
    //   int maxDepth = 30;
    //   int minData = 5;
    //   int nSplits = 30;
    //   int nThresholds = 10;

    //    int nThreads = 6;
    //    int width = 360;
    //    int height = 240;
    //    std::vector<int> binSizes = {10, 20, 20};
    //    std::vector<int> steps = {20, 10};
    //    int votesDeleteStep = 50;
    //    int votesBufferLength = 200;
    //    std::vector<double> scoreThresholds(6, 0.01);
    //    double iouThreshold = 0.1;

    //   // extractPositiveFeatures(segmentedVideoDirectoryPath, featureDirectoryPath, localWidth,
    //   //                        localHeight, localDuration, xBlockSize, yBlockSize, tBlockSize,
    //   xStep,
    //   //                        yStep, tStep, nPositiveSamplesPerStep);
    //   int beginIndex = 1;
    //   int endIndex = 20;
    //   // extractNegativeFeatures(videoDirectoryPath, labelFilePath, featureDirectoryPath,
    //   localWidth,
    //   //                        localHeight, localDuration, xBlockSize, yBlockSize, tBlockSize,
    //   xStep,
    //   //                        yStep, tStep, scales, nNegativeSamplesPerStep, beginIndex,
    //   endIndex);
    //   int beginValidationIndex = 0;
    //   int endValidationIndex = 10;
    //   // train(featureDirectoryPath, labelFilePath, forestsDirectoryPath, baseScale, nTrees,
    //   //      bootstrapRatio, maxDepth, minData, nSplits, nThresholds, beginValidationIndex,
    //   //      endValidationIndex);
    //   // detectAll(forestsDirectoryPath, votingDirectoryPath, videoDirectoryPath,
    //   // durationDirectoryPath,
    //   //          aspectDirectoryPath, localWidth, localHeight, localDuration, xBlockSize,
    //   yBlockSize,
    //   //          tBlockSize, xStep, yStep, tStep, scales, nThreads, width, height, baseScale,
    //   //          binSizes,
    //   //          votesDeleteStep, votesBufferLength, scoreThresholds, iouThreshold,
    //   //          beginValidationIndex, endValidationIndex);

    ////const cv::String keys =
    ////        "{s scoreth||score threshold}"
    ////        "{i seq||sequence index}"
    ////        "{v val||validation index}";
    ////cv::CommandLineParser parser(argc, argv, keys);
    ////std::string forestPath =
    ////        rootDirectoryPath + "data_hf/forests_hf_pooling_half_feature2_integral/" +
    /// parser.get<std::string>("v") + "/";
    ////std::string videoPath = rootDirectoryPath + "unsegmented_half/seq" +
    /// parser.get<std::string>("i") + ".avi";
    ////scoreThresholds = std::vector<double>(6, parser.get<double>("s"));
    ////detect(forestPath, videoPath, localWidth, localHeight, localDuration, xBlockSize,
    /// yBlockSize,
    ////	   tBlockSize, xStep, yStep, tStep, scales, nThreads, width, height, baseScale,
    ////	   binSizes, votesDeleteStep, votesBufferLength, scoreThresholds, iouThreshold);

    //   const cv::String keys =
    //           "{s scoreth||score threshold}"
    //           "{f fps||fps}"
    //           "{w width||width}"
    //           "{h height||height}";
    //   cv::CommandLineParser parser(argc, argv, keys);

    //   std::string forestPath =
    //           rootDirectoryPath + "data_hf/forests_hf_pooling_half_feature2_integral/0/";
    //   nThreads = 6;
    //   width = parser.get<int>("w");
    //   height = parser.get<int>("h");
    //   binSizes = {10, 20, 20};
    //   steps = {20, 10};
    //   votesDeleteStep = 50;
    //   votesBufferLength = 200;
    //   scoreThresholds = std::vector<double>(6, parser.get<double>("s"));
    //   iouThreshold = 0.1;
    //   int fps = parser.get<int>("f");
    //   detectWebCamera(forestPath, localWidth, localHeight, localDuration, xBlockSize, yBlockSize,
    //                   tBlockSize, xStep, yStep, tStep, scales, nThreads, width, height,
    //                   baseScale,
    //                   binSizes, votesDeleteStep, votesBufferLength, scoreThresholds,
    //                   iouThreshold,
    //                   fps);
}