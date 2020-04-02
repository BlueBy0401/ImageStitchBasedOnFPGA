#include "opencv2/core/core.hpp"
//#include "highgui.h"
//#include "opencv2/imgproc/imgproc.hpp"
//#include "opencv2/features2d/features2d.hpp"
//#include "opencv2/nonfree/nonfree.hpp"
//#include "opencv2/legacy/legacy.hpp"
#include<opencv2/opencv.hpp>
#include<opencv2/opencv_modules.hpp>
//#include<opencv2/imgcodecs.hpp>
//#include<opencv2/features2d.hpp>
//#include "opencv2/stitching/detail/autocalib.hpp"
//#include "opencv2/stitching/detail/blenders.hpp"
//#include "opencv2/stitching/detail/camera.hpp"
//#include "opencv2/stitching/detail/exposure_compensate.hpp"
//#include "opencv2/stitching/detail/matchers.hpp"
//#include "opencv2/stitching/detail/motion_estimators.hpp"
//#include "opencv2/stitching/detail/seam_finders.hpp"
//#include "opencv2/stitching/detail/util.hpp"
//#include "opencv2/stitching/detail/warpers.hpp"
//#include "opencv2/stitching/warpers.hpp"

#include <iostream>
#include <fstream> 
#include <string>
#include <iomanip> 
using namespace cv;
using namespace std;
using namespace detail;

float scale = 2707.47f;
float k[9];
float rinv[9];
float r_kinv[9];
float k_rinv[9];
float t[3];
inline
void mapForward(float x, float y, float &u, float &v)
{
	//cout << "r_kinv: "<< r_kinv[0] << endl;
	//cout << "r_kinv: "<< r_kinv[3] << endl;
	//cout << "r_kinv: "<< r_kinv[6] << endl;
	float x_ = r_kinv[0] * x + r_kinv[1] * y + r_kinv[2];
	float y_ = r_kinv[3] * x + r_kinv[4] * y + r_kinv[5];
	float z_ = r_kinv[6] * x + r_kinv[7] * y + r_kinv[8];

	u = scale * atan2f(x_, z_);
	v = scale * y_ / sqrtf(x_ * x_ + z_ * z_);

}
inline
void mapBackward(float u, float v, float &x, float &y)
{
	u /= scale;
	v /= scale;

	float x_ = sinf(u);
	float y_ = v;
	float z_ = cosf(u);

	float z;
	x = k_rinv[0] * x_ + k_rinv[1] * y_ + k_rinv[2] * z_;
	y = k_rinv[3] * x_ + k_rinv[4] * y_ + k_rinv[5] * z_;
	z = k_rinv[6] * x_ + k_rinv[7] * y_ + k_rinv[8] * z_;

	if (z > 0) { x /= z; y /= z; }
	else x = y = -1;
}
void detectResultRoi(Size src_size, Point &dst_tl, Point &dst_br)
{
	float tl_uf = (std::numeric_limits<float>::max)();
	float tl_vf = (std::numeric_limits<float>::max)();
	float br_uf = -(std::numeric_limits<float>::max)();
	float br_vf = -(std::numeric_limits<float>::max)();

	float u, v;
	for (int y = 0; y < src_size.height; ++y)
	{
		for (int x = 0; x < src_size.width; ++x)
		{

			mapForward(static_cast<float>(x), static_cast<float>(y), u, v);
			tl_uf = (std::min)(tl_uf, u); tl_vf = (std::min)(tl_vf, v);
			br_uf = (std::max)(br_uf, u); br_vf = (std::max)(br_vf, v);
		}
	}

	dst_tl.x = static_cast<int>(tl_uf);
	dst_tl.y = static_cast<int>(tl_vf);
	dst_br.x = static_cast<int>(br_uf);
	dst_br.y = static_cast<int>(br_vf);
}

void setCameraParams(InputArray _K, InputArray _R, InputArray _T = Mat::zeros(3, 1, CV_32F))
{
	Mat K = _K.getMat(), R = _R.getMat(), T = _T.getMat();

	CV_Assert(K.size() == Size(3, 3) && K.type() == CV_32F);
	CV_Assert(R.size() == Size(3, 3) && R.type() == CV_32F);
	CV_Assert((T.size() == Size(1, 3) || T.size() == Size(3, 1)) && T.type() == CV_32F);

	Mat_<float> K_(K);
	k[0] = K_(0, 0); k[1] = K_(0, 1); k[2] = K_(0, 2);
	k[3] = K_(1, 0); k[4] = K_(1, 1); k[5] = K_(1, 2);
	k[6] = K_(2, 0); k[7] = K_(2, 1); k[8] = K_(2, 2);

	Mat_<float> Rinv = R.t();
	rinv[0] = Rinv(0, 0); rinv[1] = Rinv(0, 1); rinv[2] = Rinv(0, 2);
	rinv[3] = Rinv(1, 0); rinv[4] = Rinv(1, 1); rinv[5] = Rinv(1, 2);
	rinv[6] = Rinv(2, 0); rinv[7] = Rinv(2, 1); rinv[8] = Rinv(2, 2);

	Mat_<float> R_Kinv = R * K.inv();
	r_kinv[0] = R_Kinv(0, 0); r_kinv[1] = R_Kinv(0, 1); r_kinv[2] = R_Kinv(0, 2);
	r_kinv[3] = R_Kinv(1, 0); r_kinv[4] = R_Kinv(1, 1); r_kinv[5] = R_Kinv(1, 2);
	r_kinv[6] = R_Kinv(2, 0); r_kinv[7] = R_Kinv(2, 1); r_kinv[8] = R_Kinv(2, 2);

	Mat_<float> K_Rinv = K * Rinv;
	k_rinv[0] = K_Rinv(0, 0); k_rinv[1] = K_Rinv(0, 1); k_rinv[2] = K_Rinv(0, 2);
	k_rinv[3] = K_Rinv(1, 0); k_rinv[4] = K_Rinv(1, 1); k_rinv[5] = K_Rinv(1, 2);
	k_rinv[6] = K_Rinv(2, 0); k_rinv[7] = K_Rinv(2, 1); k_rinv[8] = K_Rinv(2, 2);

	Mat_<float> T_(T.reshape(0, 3));
	t[0] = T_(0, 0); t[1] = T_(1, 0); t[2] = T_(2, 0);
}

Rect buildMaps(Size src_size, InputArray K, InputArray R, OutputArray _xmap, OutputArray _ymap)
{
	setCameraParams(K, R);
	Point dst_tl, dst_br;
	detectResultRoi(src_size, dst_tl, dst_br);

	_xmap.create(dst_br.y - dst_tl.y + 1, dst_br.x - dst_tl.x + 1, CV_32F);
	_ymap.create(dst_br.y - dst_tl.y + 1, dst_br.x - dst_tl.x + 1, CV_32F);

	Mat xmap = _xmap.getMat(), ymap = _ymap.getMat();
	float x, y;
	for (int v = dst_tl.y; v <= dst_br.y; ++v)
	{
		for (int u = dst_tl.x; u <= dst_br.x; ++u)
		{
			mapBackward(static_cast<float>(u), static_cast<float>(v), x, y);
			xmap.at<float>(v - dst_tl.y, u - dst_tl.x) = x;
			ymap.at<float>(v - dst_tl.y, u - dst_tl.x) = y;
		}
	}

	return Rect(dst_tl, dst_br);
}
Point warp(InputArray src, InputArray K, InputArray R, int interp_mode, int border_mode,
	OutputArray dst)
{
	UMat xmap, ymap;
	cout << src.size() << endl;
	cout << K.getMat() << endl;
	cout << R.getMat() << endl;
	Rect dst_roi = buildMaps(src.size(), K, R, xmap, ymap);
	cout << dst_roi << endl;
	dst.create(dst_roi.height + 1, dst_roi.width + 1, src.type());
	Mat a, b, c, d;
	xmap.copyTo(a);
	ymap.copyTo(b);
	imwrite("xmap.bmp", xmap);
	imwrite("ymap.bmp", ymap);
	//cout << "src.type(): " << src.type() << endl;
	cout << "a.size(): " << a.size() << endl;
	cout << "b.size(): " << b.size() << endl;
	Mat addr, weight, res;  //��ַ��Ȩ�ر��˫���Բ�ֵ��Ľ��
	addr.create(1100, 1086 * 8, CV_32SC1);
	weight.create(1100, 1086 * 4, CV_32FC1);
	res.create(1100, 1086, CV_8UC3);
	Mat s;
	src.copyTo(s);
	cout << s.size() << endl;	//���ݵ�ַ���Ȩ�ر����˫���Բ�ֵ
	cout << addr.type() << endl;  //3
	cout << "a.type: " << a.type() << endl;  //16
	cout << cvFloor(a.at<float>(0, 0)) << endl;  //0
	cout << cvCeil(a.at<float>(0, 0)) << endl;  //1
	cout << cvFloor(b.at<float>(0, 0)) << endl;  //-11
	cout << cvCeil(b.at<float>(0, 0)) << endl;  //-10
	//����ַ��
	for (int row = 0; row < xmap.rows; ++row)
		for (int col = 0; col < xmap.cols; ++col)
		{
			//cout << row << " " << col << endl;
			addr.at<int>(row, col * 8) = cvFloor(b.at<float>(row, col));
			addr.at<int>(row, col * 8 + 1) = cvFloor(a.at<float>(row, col));
			addr.at<int>(row, col * 8 + 2) = cvCeil(b.at<float>(row, col));
			addr.at<int>(row, col * 8 + 3) = cvFloor(a.at<float>(row, col));
			addr.at<int>(row, col * 8 + 4) = cvFloor(b.at<float>(row, col));
			addr.at<int>(row, col * 8 + 5) = cvCeil(a.at<float>(row, col));
			addr.at<int>(row, col * 8 + 6) = cvCeil(b.at<float>(row, col));
			addr.at<int>(row, col * 8 + 7) = cvCeil(a.at<float>(row, col));

		}
	//���Ȩ�ر�
	for (int row = 0; row < a.rows; ++row)
		for (int col = 0; col < b.cols; ++col)
		{
			weight.at<float>(row, col * 4) = (cvCeil(b.at<float>(row, col)) - b.at<float>(row, col))
				*(cvCeil(a.at<float>(row, col)) - a.at<float>(row, col));  //(y1 - y)(x2 - x)
			weight.at<float>(row, col * 4 + 1) = (b.at<float>(row, col) - cvFloor(b.at<float>(row, col)))
				*(cvCeil(a.at<float>(row, col)) - a.at<float>(row, col));  //(y - y2)(x2 - x)
			weight.at<float>(row, col * 4 + 2) = (b.at<float>(row, col) - cvFloor(b.at<float>(row, col)))
				*(a.at<float>(row, col) - cvFloor(a.at<float>(row, col)));  //(y - y2)(x - x1)
			weight.at<float>(row, col * 4 + 3) = (cvCeil(b.at<float>(row, col)) - b.at<float>(row, col))
				*(a.at<float>(row, col) - cvFloor(a.at<float>(row, col)));  //(y1 - y)(x - x1)
		}

	for (int row = 0; row < res.rows; ++row)
		for (int col = 0; col < res.cols; ++col)
		{
			//cout << weight.at<float>(row, col * 4) << endl;
			//cout << weight.at<float>(row, col * 4 + 1) << endl;
			//cout << weight.at<float>(row, col * 4 + 2) << endl;
			//cout << weight.at<float>(row, col * 4 + 3) << endl;
			//cout << row << " " << col << endl;
			if (row < 12 && (addr.at<int>(row, col * 8) < 0 || addr.at<int>(row, col * 8 + 1) < 0
				|| addr.at<int>(row, col * 8 + 2) < 0 || addr.at<int>(row, col * 8 + 3) < 0 || addr.at<int>(row, col * 8 + 4) < 0
				|| addr.at<int>(row, col * 8 + 5) < 0 || addr.at<int>(row, col * 8 + 6) < 0 || addr.at<int>(row, col * 8 + 7) < 0))
			{
				res.at<Vec3b>(row, col)[0] = 0;
				res.at<Vec3b>(row, col)[1] = 0;
				res.at<Vec3b>(row, col)[2] = 0;
			}
			else if (row > 1080 && (addr.at<int>(row, col * 8) > 1100 || addr.at<int>(row, col * 8 + 1) > 1100
				|| addr.at<int>(row, col * 8 + 2) > 1100 || addr.at<int>(row, col * 8 + 3) > 1100 || addr.at<int>(row, col * 8 + 4) > 1100
				|| addr.at<int>(row, col * 8 + 5) > 1100 || addr.at<int>(row, col * 8 + 6) > 1100 || addr.at<int>(row, col * 8 + 7) > 1100))
			{
				res.at<Vec3b>(row, col)[0] = 0;
				res.at<Vec3b>(row, col)[1] = 0;
				res.at<Vec3b>(row, col)[2] = 0;
			}
			else
			{
				//cout << "aa" << endl;
				//cout << weight.at<float>(row, col * 4) << endl;
				//cout << addr.at<int>(row, col * 8) << " " << addr.at<int>(row, col * 8 + 1) << endl;
				//cout << s.at<Vec3b>(0, 435) << endl;
				//cout << s.at<Vec3b>(addr.at<int>(row, col * 8), addr.at<int>(row, col * 8 + 1))[0] << endl;
				res.at<Vec3b>(row, col)[0] =
					weight.at<float>(row, col * 4) * s.at<Vec3b>(addr.at<int>(row, col * 8), addr.at<int>(row, col * 8 + 1))[0] +
					weight.at<float>(row, col * 4 + 1) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 2), addr.at<int>(row, col * 8 + 3))[0] +
					weight.at<float>(row, col * 4 + 2) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 4), addr.at<int>(row, col * 8 + 5))[0] +
					weight.at<float>(row, col * 4 + 3) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 6), addr.at<int>(row, col * 8 + 7))[0];
				//cout << "bb" << endl;
				res.at<Vec3b>(row, col)[1] =
					weight.at<float>(row, col * 4) * s.at<Vec3b>(addr.at<int>(row, col * 8), addr.at<int>(row, col * 8 + 1))[1] +
					weight.at<float>(row, col * 4 + 1) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 2), addr.at<int>(row, col * 8 + 3))[1] +
					weight.at<float>(row, col * 4 + 2) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 4), addr.at<int>(row, col * 8 + 5))[1] +
					weight.at<float>(row, col * 4 + 3) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 6), addr.at<int>(row, col * 8 + 7))[1];
				res.at<Vec3b>(row, col)[2] =
					weight.at<float>(row, col * 4) * s.at<Vec3b>(addr.at<int>(row, col * 8), addr.at<int>(row, col * 8 + 1))[2] +
					weight.at<float>(row, col * 4 + 1) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 2), addr.at<int>(row, col * 8 + 3))[2] +
					weight.at<float>(row, col * 4 + 2) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 4), addr.at<int>(row, col * 8 + 5))[2] +
					weight.at<float>(row, col * 4 + 3) * s.at<Vec3b>(addr.at<int>(row, col * 8 + 6), addr.at<int>(row, col * 8 + 7))[2];
			}
		}
	imwrite("res.bmp", res);
	FileStorage fs("addr.coe", FileStorage::WRITE);
	fs << "addr" << addr;

	//res.copyTo(dst);
	remap(src, dst, xmap, ymap, interp_mode, border_mode);
	src.copyTo(c);
	dst.copyTo(d);
	return dst_roi.tl();
}
int main()
{
	double t1 = clock();
	cout << "t1 = " << t1 << endl;
	int num_images = 2;
	vector<Mat> imgs;    //����ͼ��
	Mat img = imread("C:\\Users\\mhhai\\Desktop\\mh\\src_image\\IFOV\\11.bmp");
	imgs.push_back(img);
	img = imread("C:\\Users\\mhhai\\Desktop\\mh\\src_image\\IFOV\\22.bmp");
	imgs.push_back(img);

	Ptr<FeaturesFinder> finder = new  OrbFeaturesFinder();    //��������Ѱ����
	//finder = new SurfFeaturesFinder();    //Ӧ��SURF����Ѱ������
	//finder = new  OrbFeaturesFinder();    //Ӧ��ORB����Ѱ������
	vector<ImageFeatures> features(num_images);    //��ʾͼ������
	cout << "a" << endl;
	for (int i = 0; i < num_images; i++)
		(*finder)(imgs[i], features[i]);    //�������
	cout << "c" << endl;

	vector<MatchesInfo> pairwise_matches;    //��ʾ����ƥ����Ϣ����
	cout << "a: " << pairwise_matches.size() << endl;
	BestOf2NearestMatcher matcher(false, 0.3f, 6, 6);    //��������ƥ������2NN����
	matcher(features, pairwise_matches);    //��������ƥ��
	cout << "b: " << pairwise_matches.size() << endl;

	double t2 = clock();
	cout << "t2=" << t2 << endl;
	//cout << "diff1=" << t2 - t1 << endl;
	HomographyBasedEstimator estimator;    //�������������
	vector<CameraParams> cameras;    //��ʾ�������
	estimator(features, pairwise_matches, cameras);    //���������������

	for (size_t i = 0; i < cameras.size(); ++i)    //ת�������ת��������������
	{
		Mat R;
		cameras[i].R.convertTo(R, CV_32F);
		cameras[i].R = R;
	}
	cout << cameras.size() << endl; //7,��Ӧ����6����
	cout << cameras[0].R << endl;
	double t3 = clock();
	cout << "t3=" << t3 << endl;
	Ptr<detail::BundleAdjusterBase> adjuster;    //����ƽ�����ȷ�������
	//adjuster = new detail::BundleAdjusterReproj();    //��ӳ������
	adjuster = new detail::BundleAdjusterRay();    //���߷�ɢ����

	adjuster->setConfThresh(1);    //����ƥ�����Ŷȣ���ֵ��Ϊ1
	(*adjuster)(features, pairwise_matches, cameras);    //��ȷ�����������
	cout << "0R: " << cameras[0].R << endl;
	cout << "0K: " << cameras[0].K() << endl;
	cout << "1R: " << cameras[1].R << endl;
	cout << "1K: " << cameras[1].K() << endl;
	//cout << "2: " << cameras[2].R << endl;
	//cout << "3: " << cameras[3].R << endl;
	//cout << "4: " << cameras[4].R << endl;
	//cout << "5: " << cameras[5].R << endl;
	//cout << "6: " << cameras[6].R << endl;
	//������ù���ƽ���Ч���ܲ�
	//��ȷ���������ԭʼ��������Ǻܴ��
	double t4 = clock();
	cout << "t4=" << t4 << endl;


	vector<Point> corners(num_images);    //��ʾӳ��任��ͼ������Ͻ�����
	vector<UMat> masks_warped(num_images);    //��ʾӳ��任���ͼ������
	vector<UMat> images_warped(num_images);    //��ʾӳ��任���ͼ��
	vector<Size> sizes(num_images);    //��ʾӳ��任���ͼ��ߴ�
	vector<Mat> masks(num_images);    //��ʾԴͼ������

	for (int i = 0; i < num_images; ++i)    //��ʼ��Դͼ������
	{
		masks[i].create(imgs[i].size(), CV_8U);    //����ߴ��С
		masks[i].setTo(Scalar::all(255));    //ȫ����ֵΪ255����ʾԴͼ����������ʹ��
	}

	Ptr<WarperCreator> warper_creator;    //����ͼ��ӳ��任������

	warper_creator = new cv::CylindricalWarper();    //����ͶӰ

	//����ͼ��ӳ��任��������ӳ��ĳ߶�Ϊ����Ľ��࣬��������Ľ��඼��ͬ
	Ptr<RotationWarper> warper = warper_creator->create(static_cast<float>(cameras[0].focal));
	cout << "scale" << cameras[0].focal << endl;
	for (int i = 0; i < num_images; ++i)
	{
		Mat_<float> K;
		cameras[i].K().convertTo(K, CV_32F);    //ת������ڲ�������������
		//�Ե�ǰͼ����ͶӰ�任���õ��任���ͼ���Լ���ͼ������Ͻ�����
		corners[i] = warp(imgs[i], K, cameras[i].R, INTER_LINEAR, BORDER_REFLECT, images_warped[i]);
		cout << corners[i] << endl;
		sizes[i] = images_warped[i].size();    //�õ��ߴ�
		//�õ��任���ͼ������
		warp(masks[i], K, cameras[i].R, INTER_NEAREST, BORDER_CONSTANT, masks_warped[i]);
	}
	imwrite("images1_warped[0].bmp", images_warped[0]);
	imwrite("images2_warped[1].bmp", images_warped[1]);
	double t6 = clock();
	cout << "t6=" << t6 << endl;
	imgs.clear();    //�����
	masks.clear();

	//�����عⲹ������Ӧ�����油������
	Ptr<ExposureCompensator> compensator =
		ExposureCompensator::createDefault(ExposureCompensator::GAIN);
	compensator->feed(corners, images_warped, masks_warped);    //�õ��عⲹ����
	for (int i = 0; i < num_images; ++i)    //Ӧ���عⲹ��������ͼ������عⲹ��
	{
		compensator->apply(i, corners[i], images_warped[i], masks_warped[i]);
	}

	//�ں��棬���ǻ���Ҫ�õ�ӳ��任ͼ������masks_warped���������Ϊ�ñ������һ������masks_seam
	vector<UMat> masks_seam(num_images);
	for (int i = 0; i < num_images; i++)
		masks_warped[i].copyTo(masks_seam[i]);
	Ptr<SeamFinder> seam_finder;    //����ӷ���Ѱ����
   //seam_finder = new NoSeamFinder();    //����Ѱ�ҽӷ���
	//seam_finder = new VoronoiSeamFinder();    //��㷨
	//seam_finder��һ����
	//seam_finder = new DpSeamFinder(DpSeamFinder::COLOR);    //��̬�淶��
	//seam_finder = new DpSeamFinder(DpSeamFinder::COLOR_GRAD);
	//ͼ�
	seam_finder = new GraphCutSeamFinder(GraphCutSeamFinder::COST_COLOR);
	//seam_finder = new GraphCutSeamFinder(GraphCutSeamFinder::COST_COLOR_GRAD);
	vector<UMat> images_warped_f(num_images);
	for (int i = 0; i < num_images; ++i)    //ͼ����������ת��
		images_warped[i].convertTo(images_warped_f[i], CV_32F);
	//�õ��ӷ��ߵ�����ͼ��masks_warped

	seam_finder->find(images_warped_f, corners, masks_warped);
	imwrite("masks_warped[0]2.bmp", masks_warped[0]);
	imwrite("masks_warped[1]3.bmp", masks_warped[1]);
	/*
	cout << corners[0] << endl;
	cout << corners[1] << endl;
	imwrite("images_warped_f[0].bmp", images_warped_f[0]);
	imwrite("images_warped_f[1].bmp", images_warped_f[1]);
	imwrite("mask_warped[0].bmp", masks_warped[0]);
	imwrite("mask_warped[1].bmp", masks_warped[1]);

	//ͨ��canny��Ե��⣬�õ�����߽磬������һ���߽���ǽӷ���
	for (int k = 0; k < 2; k++)
		Canny(masks_warped[k], masks_warped[k], 3, 9, 3);

	//Ϊ��ʹ�ӷ��߿��ø����������ʹ���������������Ӵֱ߽���
	vector<Mat> dilate_img(2);
	Mat element = getStructuringElement(MORPH_RECT, Size(10, 10));    //����ṹԪ��
	vector<Mat> a(2);
	for (int k = 0; k < 2; k++)    //��������ͼ��
	{
		images_warped[k].copyTo(a[k]);
		dilate(masks_warped[k], dilate_img[k], element);    //��������
		//��ӳ��任ͼ�ϻ����ӷ��ߣ�������ֻ��Ϊ�˳��ֳ���һ��Ч�������Բ�û�����ֽӷ��ߺ���������߽�
		for (int y = 0; y < images_warped[k].rows; y++)
		{
			for (int x = 0; x < images_warped[k].cols; x++)
			{
				if (dilate_img[k].at<uchar>(y, x) == 255)    //����߽�
				{
					//images_warped[k].at<Vec3b>(y, x)[0] = 255;
					//images_warped[k].at<Vec3b>(y, x)[1] = 0;
					//images_warped[k].at<Vec3b>(y, x)[2] = 255;
					a[k].at<Vec3b>(y, x)[0] = 255;
					a[k].at<Vec3b>(y, x)[1] = 0;
					a[k].at<Vec3b>(y, x)[2] = 255;
				}
			}
		}
	}

	imwrite("seam1.jpg", a[0]);    //�洢ͼ��
	imwrite("seam2.jpg", a[1]);
	*/
	vector<Mat> images_warped_s(num_images);
	Ptr<Blender> blender;    //����ͼ���ں���



	//blender = Blender::createDefault(Blender::MULTI_BAND, false);    //��Ƶ���ں�
	//MultiBandBlender* mb = dynamic_cast<MultiBandBlender*>(static_cast<Blender*>(blender));
	//mb->setNumBands(4);   //����Ƶ������������������


	blender = Blender::createDefault(Blender::NO, false);    //���ںϷ���
	//���ںϷ���
	blender = Blender::createDefault(Blender::FEATHER, false);
	FeatherBlender* fb = dynamic_cast<FeatherBlender*>(static_cast<Blender*>(blender));
	fb->setSharpness(0.1);    //���������
	cout << "aaa" << endl;
	cout << sizes[0] << endl;
	cout << sizes[1] << endl;
	blender->prepare(corners, sizes);    //����ȫ��ͼ������

	//���ںϵ�ʱ������Ҫ�����ڽӷ���������д�������һ����Ѱ�ҽӷ��ߺ�õ�������ı߽���ǽӷ��ߴ���������ǻ���Ҫ�ڽӷ������࿪��һ�����������ںϴ�����һ������̶��𻯷�����Ϊ�ؼ�
	//Ӧ�������㷨��С�������
	vector<Mat> dilate_img(num_images);
	Mat element = getStructuringElement(MORPH_RECT, Size(20, 20));    //����ṹԪ��
	vector<Mat> a(num_images);
	vector<Mat> b(num_images);
	vector<Mat> c(num_images);
	cout << "corners[0]: " << corners[0] << endl;
	cout << "corners[1]: " << corners[1] << endl;

	for (int k = 0; k < num_images; k++)
	{
		images_warped_f[k].convertTo(images_warped_s[k], CV_16S);    //�ı���������
		dilate(masks_seam[k], masks_seam[k], element);    //��������
		//ӳ��任ͼ����������ͺ�������ࡰ�롱���Ӷ�ʹ��չ������������ڽӷ������࣬�����߽紦����Ӱ��
		masks_seam[k].copyTo(a[k]);
		masks_warped[k].copyTo(b[k]);
		//masks_seam[k] = masks_seam[k] & masks_warped[k];
		c[k] = a[k] & b[k];
		c[k].copyTo(masks_seam[k]);
		blender->feed(images_warped_s[k], masks_seam[k], corners[k]);    //��ʼ������
	}


	masks_seam.clear();    //���ڴ�
	images_warped_s.clear();
	masks_warped.clear();
	images_warped_f.clear();

	Mat result, result_mask;
	//����ںϲ������õ�ȫ��ͼ��result����������result_mask
	blender->blend(result, result_mask);

	imwrite("pano.jpg", result);    //�洢ȫ��ͼ��
	double t7 = clock();
	cout << "t7=" << t7 << endl;
	system("pause");
	return 0;
}




