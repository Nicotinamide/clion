#include "FaceProcessor.h"
#include <iostream>
#include <iomanip>

void testBasicPathLength() {
    std::cout << "=== Basic Path Length Test ===" << std::endl;
    
    FaceProcessor processor;
    SprayPath testPath;
    
    // Test 1: 10mm straight line
    testPath.points.clear();
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));
    
    double length1 = processor.calculatePathLength(testPath);
    std::cout << "10mm line length: " << std::fixed << std::setprecision(6) << length1 << std::endl;
    
    // Test 2: 3-4-5 triangle (5mm hypotenuse)
    testPath.points.clear();
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(3, 4, 0), gp_Dir(0, 0, 1)));
    
    double length2 = processor.calculatePathLength(testPath);
    std::cout << "3-4-5 triangle length: " << std::fixed << std::setprecision(6) << length2 << std::endl;
    
    // Analyze units
    std::cout << "\n=== Unit Analysis ===" << std::endl;
    if (length1 > 9.99 && length1 < 10.01) {
        std::cout << "Units appear to be: millimeters (mm)" << std::endl;
        std::cout << "Recommended threshold: 20.0" << std::endl;
    } else if (length1 > 0.0099 && length1 < 0.0101) {
        std::cout << "Units appear to be: meters (m)" << std::endl;
        std::cout << "Recommended threshold: 0.02" << std::endl;
    } else if (length1 > 0.99 && length1 < 1.01) {
        std::cout << "Units appear to be: centimeters (cm)" << std::endl;
        std::cout << "Recommended threshold: 2.0" << std::endl;
    } else {
        std::cout << "Unknown units. Measured length: " << length1 << std::endl;
        std::cout << "Please check your STEP file units" << std::endl;
    }
}

void testThresholdFiltering() {
    std::cout << "\n=== Threshold Filtering Test ===" << std::endl;
    
    FaceProcessor processor;
    
    // Test different thresholds
    double thresholds[] = {0.02, 2.0, 20.0, 200.0};
    int numThresholds = sizeof(thresholds) / sizeof(thresholds[0]);
    
    for (int i = 0; i < numThresholds; i++) {
        processor.setMinPathLength(thresholds[i]);
        std::cout << "Threshold set to: " << thresholds[i] << std::endl;
        
        // Create test paths of different lengths
        SprayPath shortPath, mediumPath, longPath;
        
        // Short path: 1 unit
        shortPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
        shortPath.points.push_back(PathPoint(gp_Pnt(1, 0, 0), gp_Dir(0, 0, 1)));
        double shortLength = processor.calculatePathLength(shortPath);
        
        // Medium path: 10 units
        mediumPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
        mediumPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));
        double mediumLength = processor.calculatePathLength(mediumPath);
        
        // Long path: 100 units
        longPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
        longPath.points.push_back(PathPoint(gp_Pnt(100, 0, 0), gp_Dir(0, 0, 1)));
        double longLength = processor.calculatePathLength(longPath);
        
        std::cout << "  Short path (" << shortLength << "): " 
                  << (shortLength >= thresholds[i] ? "KEEP" : "FILTER") << std::endl;
        std::cout << "  Medium path (" << mediumLength << "): " 
                  << (mediumLength >= thresholds[i] ? "KEEP" : "FILTER") << std::endl;
        std::cout << "  Long path (" << longLength << "): " 
                  << (longLength >= thresholds[i] ? "KEEP" : "FILTER") << std::endl;
        std::cout << std::endl;
    }
}

void printRecommendations() {
    std::cout << "=== Recommendations ===" << std::endl;
    std::cout << "1. Run this test to determine your units" << std::endl;
    std::cout << "2. Set appropriate threshold based on unit analysis" << std::endl;
    std::cout << "3. Common thresholds:" << std::endl;
    std::cout << "   - Millimeters: 20.0 (20mm)" << std::endl;
    std::cout << "   - Centimeters: 2.0 (2cm = 20mm)" << std::endl;
    std::cout << "   - Meters: 0.02 (0.02m = 20mm)" << std::endl;
    std::cout << "   - Inches: 0.787 (0.787in ≈ 20mm)" << std::endl;
    std::cout << "4. Test filtering with your actual data" << std::endl;
}

int main() {
    testBasicPathLength();
    testThresholdFiltering();
    printRecommendations();
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}
