#include <cstdio>
#include "circle_detector.h"
using namespace std;

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

#define MAX_SEGMENTS 10000

//Variable initialization
cv::CircleDetector::CircleDetector(int _width,int _height, int _color_precision, int _color_step)
{
  color_precision = _color_precision;
  color_step = _color_step;
        
	debug = false; 
	draw = false; 
	drawAll = true;
	maxFailed = 0;
	minSize = 10;
	thresholdStep = 256;
	maxThreshold = 3*256;
	centerDistanceToleranceRatio = 0.1;
	centerDistanceToleranceAbs = 5;
	circularTolerance = 0.3;
	ratioTolerance = 0.4;
	threshold = maxThreshold/2;       
	numFailed = maxFailed;
	track = true;

	//initialization - fixed params
	width = _width;
	height = _height;
	len = width*height;
	siz = len*3;
  buffer.resize(len);
  queue.resize(len);
	float diameterRatio = 5.0/14;
	//diameterRatio = 3.1/7.0;
	//diameterRatio = 7/20.5;
	float areaRatioInner_Outer = diameterRatio*diameterRatio;
	outerAreaRatio = M_PI*(1.0-areaRatioInner_Outer)/4;
	innerAreaRatio = M_PI/4.0;
	areasRatio = (1.0-areaRatioInner_Outer)/areaRatioInner_Outer;

	tima = timb = timc =timd = sizer = sizerAll = 0;
  segmentArray.resize(MAX_SEGMENTS);
}

cv::CircleDetector::~CircleDetector()
{
	printf("Timi %i %i %i %i\n",tima,timb,sizer,sizerAll);
}

bool cv::CircleDetector::changeThreshold()
{
	int div = 1;
	int dum = numFailed;
	while (dum > 1){
		dum = dum/2;
		div*=2;
	}
	int step = 256/div;
	threshold = 3*(step*(numFailed-div)+step/2);
	fprintf(stdout,"Threshold: %i %i %i\n",div,numFailed,threshold/3);
	return step > 16;
}

bool cv::CircleDetector::examineCircle(const cv::Mat& image, cv::CircleDetector::Circle& circle, int ii,float areaRatio)
{
	//timer.reset();
	//timer.start();
	int vx,vy;
	queueOldStart = queueStart;
	int position = 0;
	int pos;	
	bool result = false;
	int type = buffer[ii];
	int maxx,maxy,minx,miny;

	buffer[ii] = ++numSegments;
	circle.x = ii%width; 
	circle.y = ii/width;
	minx = maxx = circle.x;
	miny = maxy = circle.y;
	circle.valid = false;
	circle.round = false;
	//push segment coords to the queue
	queue[queueEnd++] = ii;
	//and until queue is empty
	while (queueEnd > queueStart){
		//pull the coord from the queue
		position = queue[queueStart++];
		//search neighbours
		pos = position+1;
		if (buffer[pos] == 0){
			 ptr = &image.data[pos*3];
			 buffer[pos]=((ptr[0]+ptr[1]+ptr[2]) > threshold)-2;
		}
		if (buffer[pos] == type){
			queue[queueEnd++] = pos;
			maxx = max(maxx,pos%width);
			buffer[pos] = numSegments;
		}
		pos = position-1;
		if (buffer[pos] == 0){
			 ptr = &image.data[pos*3];
			 buffer[pos]=((ptr[0]+ptr[1]+ptr[2]) > threshold)-2;
		}
		if (buffer[pos] == type){
			queue[queueEnd++] = pos;
			minx = min(minx,pos%width);
			buffer[pos] = numSegments;
		}
		pos = position-width;
		if (buffer[pos] == 0){
			 ptr = &image.data[pos*3];
			 buffer[pos]=((ptr[0]+ptr[1]+ptr[2]) > threshold)-2;
		}
		if (buffer[pos] == type){
			queue[queueEnd++] = pos;
			miny = min(miny,pos/width);
			buffer[pos] = numSegments;
		}
		pos = position+width;
		if (buffer[pos] == 0){
			 ptr = &image.data[pos*3];
			 buffer[pos]=((ptr[0]+ptr[1]+ptr[2]) > threshold)-2;
		}
		if (buffer[pos] == type){
			queue[queueEnd++] = pos;
			maxy = max(maxy,pos/width);
			buffer[pos] = numSegments;
		}
	}

	//once the queue is empty, i.e. segment is complete, we compute its size 
	circle.size = queueEnd-queueOldStart;
	if (circle.size > minSize){
		//and if its large enough, we compute its other properties 
		circle.maxx = maxx;
		circle.maxy = maxy;
		circle.minx = minx;
		circle.miny = miny;
		circle.type = -type;
		vx = (circle.maxx-circle.minx+1);
		vy = (circle.maxy-circle.miny+1);
		circle.x = (circle.maxx+circle.minx)/2;
		circle.y = (circle.maxy+circle.miny)/2;
		circle.roundness = vx*vy*areaRatio/circle.size;
		//we check if the segment is likely to be a ring 
		if (circle.roundness - circularTolerance < 1.0 && circle.roundness + circularTolerance > 1.0)
		{
			//if its round, we compute yet another properties 
			circle.round = true;
			circle.mean = 0;
			for (int p = queueOldStart;p<queueEnd;p++){
				pos = queue[p];
				circle.mean += image.data[pos*3]+image.data[pos*3+1]+image.data[pos*3+2];
			}
			circle.mean = circle.mean/circle.size;
			result = true;	
		}
	}
	//timb +=timer.getTime();
	return result;
}

cv::CircleDetector::Circle cv::CircleDetector::detect(const cv::Mat& image, const cv::CircleDetector::Circle& previous_circle)
{
	int tx,ty;
	Circle result;
	numSegments = 0;

	//image thresholding 
	//timer.reset();
	//timer.start();
	memset(&buffer[0],0,sizeof(int)*len);
	//tima += timer.getTime();
  
	//image delimitation
	int pos = (height-1)*width;
	for (int i = 0;i<width;i++){
		buffer[i] = -1000;	
		buffer[pos+i] = -1000;
	}
	for (int i = 0;i<height;i++){
		buffer[width*i] = -1000;	
		buffer[width*i+width-1] = -1000;
	}

	int ii = 0;
	int start = 0;
	bool cont = true;

	if (previous_circle.valid && track){
		ii = previous_circle.y*width+previous_circle.x;
		start = ii;
	}
  
	while (cont) 
	{
		if (buffer[ii] == 0){
			ptr = &image.data[ii*3];
			buffer[ii]=((ptr[0]+ptr[1]+ptr[2]) > threshold)-2;
		}
		if (numSegments < MAX_SEGMENTS && buffer[ii] == -2){
			//new segment found
			queueEnd = 0;
			queueStart = 0;
			//if the segment looks like a ring, we check its inside area
			if (examineCircle(image,segmentArray[numSegments],ii,outerAreaRatio)){
				pos = segmentArray[numSegments-1].y*width+segmentArray[numSegments-1].x;
				if (buffer[pos] == 0){
					ptr = &image.data[pos*3];
					buffer[pos]=((ptr[0]+ptr[1]+ptr[2]) > threshold)-2;
				}
				if (numSegments < MAX_SEGMENTS && buffer[pos] == -1){
					if (examineCircle(image,segmentArray[numSegments],pos,innerAreaRatio)){
						//the inside area is a circle. now what is the area ratio of the black and white ? also, are the circles concentric ?

						if (debug) printf("Area ratio %i/%i is %.3f %.3f %.3f\n",numSegments-2,numSegments-1,(float)segmentArray[numSegments-2].size/segmentArray[numSegments-1].size,areasRatio,segmentArray[numSegments-2].size/areasRatio/segmentArray[numSegments-1].size);
						if (
								((float)segmentArray[numSegments-2].size/areasRatio/(float)segmentArray[numSegments-1].size - ratioTolerance < 1.0 && (float)segmentArray[numSegments-2].size/areasRatio/(float)segmentArray[numSegments-1].size + ratioTolerance > 1.0) && 
								(fabsf(segmentArray[numSegments-1].x-segmentArray[numSegments-2].x) <= centerDistanceToleranceAbs+centerDistanceToleranceRatio*((float)(segmentArray[numSegments-2].maxx-segmentArray[numSegments-2].minx))) &&
								(fabsf(segmentArray[numSegments-1].y-segmentArray[numSegments-2].y) <= centerDistanceToleranceAbs+centerDistanceToleranceRatio*((float)(segmentArray[numSegments-2].maxy-segmentArray[numSegments-2].miny)))

						   ){
							int cm0,cm1,cm2;
							cm0=cm1=cm2=0;
							segmentArray[numSegments-1].x = segmentArray[numSegments-2].x;
							segmentArray[numSegments-1].y = segmentArray[numSegments-2].y;

							float sx = 0;
							int ss = segmentArray[numSegments-2].size;
							for (int p = 0;p<ss;p++){
								pos = queue[p];
								sx += pos%width;
							}
							segmentArray[numSegments-2].x = sx/ss;
							sx = 0;
							for (int p = queueOldStart;p<queueEnd;p++){
								pos = queue[p];
								sx += pos%width;
							}
							segmentArray[numSegments-1].x = sx/(queueEnd-queueOldStart);

							for (int p = 0;p<queueEnd;p++){
								pos = queue[p];
								tx = pos%width-segmentArray[numSegments-2].x;
								ty = pos/width-segmentArray[numSegments-2].y;
								cm0+=tx*tx; 
								cm2+=ty*ty; 
								cm1+=tx*ty; 
							}
							float fm0,fm1,fm2;
							fm0 = ((float)cm0)/queueEnd;
							fm1 = ((float)cm1)/queueEnd;
							fm2 = ((float)cm2)/queueEnd;
							float f0 = ((fm0+fm2)+sqrtf((fm0+fm2)*(fm0+fm2)-4*(fm0*fm2-fm1*fm1)))/2;
							float f1 = ((fm0+fm2)-sqrtf((fm0+fm2)*(fm0+fm2)-4*(fm0*fm2-fm1*fm1)))/2;
							segmentArray[numSegments-1].m0 = sqrtf(f0);
							segmentArray[numSegments-1].m1 = sqrtf(f1);
							segmentArray[numSegments-1].v0 = -fm1/sqrtf(fm1*fm1+(fm0-f0)*(fm0-f0));
							segmentArray[numSegments-1].v1 = (fm0-f0)/sqrtf(fm1*fm1+(fm0-f0)*(fm0-f0));
							segmentArray[numSegments-1].bwRatio = (float)segmentArray[numSegments-2].size/segmentArray[numSegments-1].size;

							if (track) ii = start -1;
							sizer+=segmentArray[numSegments-2].size + segmentArray[numSegments-1].size;
							sizerAll+=len;
							segmentArray[numSegments-2].valid = segmentArray[numSegments-1].valid = true;
							threshold = (segmentArray[numSegments-2].mean+segmentArray[numSegments-1].mean)/2;
						}
					}
				}
			}
		}
		ii++;
		if (ii >= len) ii = 0;
		cont = (ii != start);
	}
	/*fprintf(stdout,"Numsegments: %i\n",numSegments);
	fprintf(stdout,"Pos: %.2f \n",segmentArray[0].x-segmentArray[1].x);*/
	for (int i = 0;i< numSegments;i++){
		if (segmentArray[i].size > minSize && (segmentArray[i].valid || debug)){
			if (debug)fprintf(stdout,"Segment %i Type: %i Pos: %.2f %.2f Area: %i Vx: %i Vy: %i Mean: %i Thr: %i Eigen: %03f %03f Roundness: %03f\n",i,segmentArray[i].type,segmentArray[i].x,segmentArray[i].y,segmentArray[i].size,segmentArray[i].maxx-segmentArray[i].minx,segmentArray[i].maxy-segmentArray[i].miny,segmentArray[i].mean,threshold,segmentArray[i].m0,segmentArray[i].m1,segmentArray[i].roundness);
			if (segmentArray[i].valid) { result = segmentArray[i]; break; }
		}
	}

	//Drawing results 
	if (result.valid){
		lastThreshold = threshold;
		drawAll = false;
		numFailed = 0;	
	}else if (numFailed < maxFailed){
		if (numFailed++%2 == 0) changeThreshold(); else threshold = lastThreshold;
		if (debug) drawAll = true;
	}else{
		numFailed++;
		if (changeThreshold()==false) numFailed = 0;
		if (debug) drawAll = true;
	}
  if (draw) {
    for (int i = 0;i<len;i++){
      int j = buffer[i];
      if (j > 0){
        if (drawAll || segmentArray[j-1].valid){
          image.data[i*3+j%3] = 0;
          image.data[i*3+(j+1)%3] = 0;
          image.data[i*3+(j+2)%3] = 0;
        }
      }
    }
  }
	return result;
}