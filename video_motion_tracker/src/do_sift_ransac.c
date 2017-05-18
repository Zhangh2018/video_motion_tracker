/*
* Copyright (c) 2017 Rockchip Electronics Co. Ltd.
* Do sift&ransac algo for each pair of frame
* Author: SYJ
*/

#include "do_sift_ransac.h"

void do_sift_ransac_algo(char* frame_address,char* result_address,int frame_cnt,int direct)
{
	char* frame_name_1[256];
	char* frame_name_2[256];
	char* save_name[256];

	for (int i = 1; i <= frame_cnt; i++) {
		printf("\nframe:%d\n", i);
		sprintf(frame_name_1, "%s%s%d%s", frame_address, "\\frame", i, ".jpg");
		sprintf(frame_name_2, "%s%s%d%s", frame_address, "\\frame", i + 1, ".jpg");
		sprintf(save_name, "%s%s%d%s%s", frame_address, "\\frame", i, "_1", ".jpg");
		printf("%s\n%s\n%s\n", frame_name_1, frame_name_2,save_name);

		IplImage* img1, *img2;
		img1 = cvLoadImage(frame_name_1, 1);
		img2 = cvLoadImage(frame_name_2, 1);
		int depth = img1->width;
		int channels = img1->height;

		if (img1 == NULL || img2 == NULL)
		{
			printf("file not exist or end of sequence");
			return;
		}

		match(img1, img2, save_name, result_address, i,direct);

		cvReleaseImage(&img1);
		cvReleaseImage(&img2);
	}
}

void match(IplImage *img1, IplImage *img2, char* savename, char* txtFile, int t_time,int direct)
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

	CvPoint pt1, pt2;//���ߵ������˵�  
	double d0, d1;//feat1��ÿ�������㵽����ںʹν��ڵľ���  
	matchNum = 0;//�������ֵ��ɸѡ���ƥ���Եĸ���  
	IplImage  *stacked_ransac;

	stacked_ransac = stack_imgs(img1, img2);//�ϳ�ͼ����ʾ��RANSAC�㷨ɸѡ���ƥ����  
											
	kd_root = kdtree_build(feat2, n2);//����ͼ2�������㼯feat2����k-d��������k-d������kd_root  
	if (n1 > 1500)
		n1 = 1500;

	//���������㼯feat1�����feat1��ÿ��������feat��ѡȡ���Ͼ����ֵ������ƥ��㣬�ŵ�feat��fwd_match����  
	for (int i = 0; i < n1; i++)
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
#ifdef DRAW_RESULT_NO_RANSAC
				pt1 = cvPoint(cvRound(feat->x), cvRound(feat->y));
				pt2 = cvPoint(cvRound(nbrs[0]->x), cvRound(nbrs[0]->y));
				pt2.y += img1->height;//��������ͼ���������еģ�pt2�����������ͼ1�ĸ߶ȣ���Ϊ���ߵ��յ�  
				cvLine(stacked_ransac, pt1, pt2, CV_RGB(255, 0, 255), 1, 8, 0);//��������  
#endif
				matchNum++;//ͳ��ƥ���Եĸ���  
				feat1[i].fwd_match = nbrs[0];//ʹ��feat��fwd_match��ָ�����Ӧ��ƥ���  
			}
		}
		free(nbrs);//�ͷŽ�������  
	}

	const char *IMG_MATCH2 = "stacked_ransac";
	printf("SIFTƥ���Ը�����%d\n", matchNum);

	//����RANSAC�㷨ɸѡƥ���,����任����H  
	CvMat * H = ransac_xform(feat1, n1, FEATURE_FWD_MATCH, lsq_homog, 4, 0.01, homog_xfer_err, 3.0, &inliers, &n_inliers);

	printf("��RANSACȥ���ƥ���Ը�����%d\n", n_inliers);

	double transation = 0.0;
	int count = 0;
	//������RANSAC�㷨ɸѡ��������㼯��inliers���ҵ�ÿ���������ƥ��㣬��������  
	for (int i = 0; i < n_inliers; i++)
	{
		feat = inliers[i];//��i��������  
		//if (cvRound(feat->y) < 480 || cvRound(feat->y) > 500 && cvRound(feat->y) < 980) {//����ͼ���в���ƥ�������
			pt1 = cvPoint(cvRound(feat->x), cvRound(feat->y));//ͼ1�е������  
			pt2 = cvPoint(cvRound(feat->fwd_match->x), cvRound(feat->fwd_match->y));//ͼ2�е������(feat��ƥ���)  
			if (direct == 0)
				transation = transation + feat->x - feat->fwd_match->x;
			else
				transation = transation + feat->y - feat->fwd_match->y;

			pt2.y += img1->height;//��������ͼ���������еģ�pt2�����������ͼ1�ĸ߶ȣ���Ϊ���ߵ��յ�  
			cvLine(stacked_ransac, pt1, pt2, CV_RGB(0, 255, 0), 1, 8, 0);//��������  
			count++;
		//}
	}
	if (count == 0)
		count = 1;

	double tranX = transation / count*1.0;
	printf("λ����:%lf\n", tranX);

	FILE* file;
	file = fopen(txtFile, "w");
	fprintf(file, "%d%s", t_time, ",");
	fprintf(file, "%lf\n", tranX);
	fclose(file);

	cvSaveImage(savename, stacked_ransac, 0);
	cvReleaseImage(&stacked_ransac);
	free(feat1);
	free(feat2);
	free(inliers);
	free(kd_root);
}