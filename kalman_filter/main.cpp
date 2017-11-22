#include <iostream>
#include <fstream>
#include <string>
//#include <iterator>
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/video/tracking.hpp>

int main()
{
    // ---------------------------------------------------
    // OpenCL Initialization
    // ---------------------------------------------------
    // check OpenCL availability
    if (!cv::ocl::haveOpenCL())
    {
        std::cout << "OpenCL is not avaiable..." << std::endl;
        return -1;
    }

    // create context
    cv::ocl::Context context;
    if (!context.create(cv::ocl::Device::TYPE_GPU))
    {
        std::cout << "Failed creating the context..." << std::endl;
        return -1;
    }

    // device detection
    std::cout << context.ndevices() << " GPU devices are detected." << std::endl;
    for (int i = 0; i < context.ndevices(); i++)
    {
        cv::ocl::Device device = context.device(i);
        std::cout << "name                 : " << device.name() << std::endl;
        std::cout << "available            : " << device.available() << std::endl;
        std::cout << "imageSupport         : " << device.imageSupport() << std::endl;
        std::cout << "OpenCL_C_Version     : " << device.OpenCL_C_Version() << std::endl;
    }

    // Select the first device
    cv::ocl::Device(context.device(0));

    // ---------------------------------------------------
    // Data Initialization
    // ---------------------------------------------------
    // Kalman filter here
    cv::KalmanFilter KF(16, 8, 0);
    // KF.transitionMatrix needs to be CV_32F or CV_64F
    std::cout << "KF.transitionMatrix(A): \n" << KF.transitionMatrix << std::endl;    
    cv::Mat inputMeas = cv::Mat::ones(16, 1, CV_32F);
    cv::Mat mat_dst = cv::Mat::ones(16, 1, CV_32F);
    std::cout << "KF.transitionMatrix: \n" << KF.transitionMatrix << std::endl;    
    std::cout << "inputMeas: \n" << inputMeas << std::endl;
    
    // Transfer Mat data to the device
    cv::UMat utransitionMatrix = KF.transitionMatrix.getUMat(cv::ACCESS_READ, cv::USAGE_ALLOCATE_DEVICE_MEMORY);
    cv::UMat uinputMeas = inputMeas.getUMat(cv::ACCESS_READ, cv::USAGE_ALLOCATE_DEVICE_MEMORY);    
    cv::UMat umat_dst(mat_dst.size(), mat_dst.type(), cv::ACCESS_WRITE, cv::USAGE_ALLOCATE_DEVICE_MEMORY);

    // ---------------------------------------------------
    // Kernel Initialization
    // ---------------------------------------------------
    // Read the OpenCL kernel code
    std::ifstream ifs("kalman_predict.cl");
    if (ifs.fail()) 
        return -1;
    std::string kernelSource((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    cv::ocl::ProgramSource programSource(kernelSource);

    // Compile the kernel code
    cv::String errmsg;
    // By setting program argument "-D xxx=yyy ", we can replace xxx with yyy in the kernel
    cv::String buildopt =""; 
    cv::ocl::Program program = context.getProg(programSource, buildopt, errmsg);    

    // ---------------------------------------------------
    // Create Kernel and run
    // ---------------------------------------------------    
    // create kernel
    cv::ocl::Kernel kernel_statePre("kalman_predict", program);
    // kernel argument
    int Mat_type = CV_32F;    
    kernel_statePre.args(Mat_type, 
                cv::ocl::KernelArg::ReadOnlyNoSize(utransitionMatrix),
                cv::ocl::KernelArg::ReadOnlyNoSize(uinputMeas),
                cv::ocl::KernelArg::ReadWrite(umat_dst));

    size_t globalThreads[3] = { (size_t)KF.transitionMatrix.cols, (size_t)KF.transitionMatrix.rows, 1 };
    //size_t localThreads[3] = { 16, 16, 1 };

    bool success = kernel_statePre.run(3, globalThreads, NULL, true);
    if (!success){
        std::cout << "Failed running the kernel..." << std::endl;
        return -1;
    }
    // ---------------------------------------------------
    // Copy data from device(GPU) to host(CPU)
    // ---------------------------------------------------     
    // Download the dst data from the device
    mat_dst = umat_dst.getMat(cv::ACCESS_READ);
    std::cout << "\nmat_dst: \n" << mat_dst << std::endl;


    // create kernel
    cv::ocl::Kernel kernel_errorCovPre("kalman_predict", program);
    // kernel argument
    int Mat_type = CV_32F;
    kernel_errorCovPre.args(Mat_type, 
                cv::ocl::KernelArg::ReadOnlyNoSize(utransitionMatrix),
                cv::ocl::KernelArg::ReadOnlyNoSize(uinputMeas),
                cv::ocl::KernelArg::ReadWrite(umat_dst));

    size_t globalThreads[3] = { (size_t)KF.transitionMatrix.cols, (size_t)KF.transitionMatrix.rows, 1 };
    //size_t localThreads[3] = { 16, 16, 1 };

    bool success = kernel_errorCovPre.run(3, globalThreads, NULL, true);
    if (!success){
        std::cout << "Failed running the kernel..." << std::endl;
        return -1;
    }

    // Download the dst data from the device (?)
    mat_dst = umat_dst.getMat(cv::ACCESS_READ);
    std::cout << "\nmat_dst: \n" << mat_dst << std::endl;

    return 0;
}