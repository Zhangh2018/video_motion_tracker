/*
* Copyright (c) 2017 Rockchip Electronics Co. Ltd.
* Do sift&ransac algo for each pair of frame
* Author: SYJ
*/

#include "do_sift_ransac.h"

void do_sift_ransac_algo(char* frame_address, char* result_address, int frame_cnt, int direct)
{
	char* frame_name_1[256];
	char* frame_name_2[256];
	char* save_name[256];

	for (int i = 1; i <= frame_cnt; i++) {
		printf("\nframe:%d\n", i);
		sprintf(frame_name_1, "%s%s%d%s", frame_address, "\\frame", i, ".jpg");
		sprintf(frame_name_2, "%s%s%d%s", frame_address, "\\frame", i + 1, ".jpg");

		IplImage* img1, *img2;
		img1 = cvLoadImage(frame_name_1, 1);
		img2 = cvLoadImage(frame_name_2, 1);

		if (img1 == NULL || img2 == NULL)
		{
			printf("file not exist or end of sequence");
			return;
		}
#if TEN_PIECE
		for (float j = 0.0; j < 1.0; j += 0.1)
		{
			sprintf(save_name, "%s%s%.1f%s", frame_address, "\\frame", i + j, ".jpg");
			IplImage* img1_block, *img2_block;
			cvSetImageROI(img1, cvRect(0, j*img1->height, img1->width, 0.1*img1->height));
			img1_block = cvCreateImage(cvSize(img1->width, 0.1*img1->height), IPL_DEPTH_8U, img1->nChannels);
			cvCopy(img1, img1_block, 0);
			cvResetImageROI(img1);

			cvSetImageROI(img2, cvRect(0, j*img2->height, img2->width, 0.1*img2->height));
			img2_block = cvCreateImage(cvSize(img2->width, 0.1*img2->height), IPL_DEPTH_8U, img2->nChannels);
			cvCopy(img2, img2_block, 0);
			cvResetImageROI(img2);

			match(img1_block, img2_block, save_name, result_address, i + j, direct);
			cvReleaseImage(&img1_block);
			cvReleaseImage(&img2_block);
		}
#else
		match(img1, img2, save_name, result_address, i, direct);
#endif
		cvReleaseImage(&img1);
		cvReleaseImage(&img2);
	}
}

void match(IplImage *img1, IplImage *img2, char* savename, char* txtFile, float t_time, int direct)
{
	/* the maximum number of keypoint NN candidates to check during BBF search */
	int KDTREE_BBF_MAX_NN_CHKS = 200;

	/* threshold on squared ratio of distances between NN and 2nd NN */
	float NN_SQ_DIST_RATIO_THR = 0.49;

	struct feature *feat1, *feat2;//feat1��ͼ1�����������飬feat2��ͼ2������������  
	int n1 = 0, n2 = 0;//n1:ͼ1�е������������n2��ͼ2�е����������  
	struct feature *feat;//ÿ��������  
	struct kd_node *kd_root;//k-d��������  
	struct feature **nbrs;//��ǰ�����������ڵ�����  
	int matchNum = 0;//�������ֵ��ɸѡ���ƥ���Եĸ���  
	struct feature **inliers;//��RANSACɸѡ����ڵ�����  
	int n_inliers = 0;//��RANSAC�㷨ɸѡ����ڵ����,��feat1�о��з���Ҫ���������ĸ���  

	n1 = sift_features(img1, &feat1);//���ͼ1�е�SIFT������,n1��ͼ1�����������  

	n2 = sift_features(img2, &feat2);//���ͼ2�е�SIFT�����㣬n2��ͼ2�����������  

	double d0, d1;//feat1��ÿ�������㵽����ںʹν��ڵľ���  
	matchNum = 0;//�������ֵ��ɸѡ���ƥ���Եĸ��� 

#if SAVE_SIFT_IMAGE
	CvPoint pt1, pt2;//���ߵ������˵�  
	IplImage  *stacked_ransac;
	stacked_ransac = stack_imgs(img1, img2);//�ϳ�ͼ����ʾ��RANSAC�㷨ɸѡ���ƥ����  
#endif											
	kd_root = kdtree_build(feat2, n2);//����ͼ2�������㼯feat2����k-d��������k-d������kd_root  
	int n = min(n1, n2);

	//���������㼯feat1�����feat1��ÿ��������feat��ѡȡ���Ͼ����ֵ������ƥ��㣬�ŵ�feat��fwd_match����  
	for (int i = 0; i < n; i++)
	{
		feat = feat1 + i;//��i���������ָ��  
						 //��kd_root������Ŀ���feat��2������ڵ㣬�����nbrs�У�����ʵ���ҵ��Ľ��ڵ����  
		int k = kdtree_bbf_knn(kd_root, feat, 2, &nbrs, KDTREE_BBF_MAX_NN_CHKS);
		if (k == 2)
		{
			d0 = descr_dist_sq(feat, nbrs[0]);//feat������ڵ�ľ����ƽ��  
			d1 = descr_dist_sq(feat, nbrs[1]);//feat��ν��ڵ�ľ����ƽ��  
											  //��d0��d1�ı�ֵС����ֵNN_SQ_DIST_RATIO_THR������ܴ�ƥ�䣬�����޳�  

			if (d0 < d1 * NN_SQ_DIST_RATIO_THR)
			{   //��Ŀ���feat������ڵ���Ϊƥ����  
#if DRAW_RESULT_NO_RANSAC
				pt1 = cvPoint(cvRound(feat->x), cvRound(feat->y));
				pt2 = cvPoint(cvRound(nbrs[0]->x), cvRound(nbrs[0]->y));
				pt2.y += img1->height;//��������ͼ���������еģ�pt2�����������ͼ1�ĸ߶ȣ���Ϊ���ߵ��յ�  
				cvLine(stacked_ransac, pt1, pt2, CV_RGB(255, 0, 255), 1, 8, 0);//��������  
#endif
				matchNum++;//ͳ��ƥ���Եĸ���  
				feat1[i].fwd_match = nbrs[0];//ʹ��feat��fwd_match��ָ�����Ӧ��ƥ���  
			}
		}
		if (nbrs)
			free(nbrs);//�ͷŽ�������  
	}

	const char *IMG_MATCH2 = "stacked_ransac";
	printf("SIFTƥ���Ը�����%d\n", matchNum);

	//����RANSAC�㷨ɸѡƥ���,����任����H  
	CvMat * H = ransac_xform(feat1, n, FEATURE_FWD_MATCH, lsq_homog, 4, 0.01, homog_xfer_err, 3.0, &inliers, &n_inliers);

	printf("��RANSACȥ���ƥ���Ը�����%d\n", n_inliers);

	double transation = 0.0;
	int count = 0;
	//������RANSAC�㷨ɸѡ��������㼯��inliers���ҵ�ÿ���������ƥ��㣬��������  
	for (int i = 0; i < n_inliers; i++)
	{
		feat = inliers[i];//��i��������  
#if SET_ROI
		if (cvRound(feat->y) < 480 || cvRound(feat->y) > 500 && cvRound(feat->y) < 980) {//����ͼ���в���ƥ�������
#endif
			if (direct == 0)
				transation = transation + feat->x - feat->fwd_match->x;
			else
				transation = transation + feat->y - feat->fwd_match->y;
#if SAVE_SIFT_IMAGE
			pt1 = cvPoint(cvRound(feat->x), cvRound(feat->y));//ͼ1�е������  
			pt2 = cvPoint(cvRound(feat->fwd_match->x), cvRound(feat->fwd_match->y));//ͼ2�е������(feat��ƥ���)  
			pt2.y += img1->height;//��������ͼ���������еģ�pt2�����������ͼ1�ĸ߶ȣ���Ϊ���ߵ��յ�  
			cvLine(stacked_ransac, pt1, pt2, CV_RGB(0, 255, 0), 1, 8, 0);//��������  
#endif
			count++;
#if SET_ROI
		}
#endif
	}
	if (count == 0)
		count = 1;

	double tranX = transation / count*1.0;
	printf("λ����:%lf\n", tranX);

	if (tranX != 0)
	{
		FILE* file;
		file = fopen(txtFile, "a+");
		fprintf(file, "%.1f%s", t_time, ",");
		fprintf(file, "%lf\n", tranX);
		fclose(file);
	}
#if SAVE_SIFT_IMAGE
	cvSaveImage(savename, stacked_ransac, 0);
	cvReleaseImage(&stacked_ransac);
#endif
	if (n1)
		free(feat1);
	if (n2)
		free(feat2);
	if (n_inliers)
		free(inliers);
	if (n2)
		free(kd_root);
}