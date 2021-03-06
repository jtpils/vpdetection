// JLinkageLibRandomSamplerMex.cpp : mex-function interface implentation file

#include "RandomSampler.h"
#include "JLinkage.h"
#include "PrimitiveFunctions.h"
#include "../../VPPrimitive.h" //FC
#include "mex.h"
#include <ctime>


// Create the waitbar
mxArray *hw,*title,*percent;
double *percentDoublePtr;
mxArray *input_array[3];
mxArray *output_array[1];
time_t initialTime, currentTime;
char tempString[128];


void InitializeWaitbar(){
	// Initialize waitbar
	title = mxCreateString("In progress ...");
	percent = mxCreateDoubleMatrix(1,1, mxREAL);
	percentDoublePtr = (double *)mxGetPr(percent);
	*percentDoublePtr = 0.0;

	input_array[0] = percent;
	input_array[1] = title;

	mexCallMATLAB(1, output_array, 2, input_array, "waitbar");
	hw = output_array[0];
	input_array[1] = hw;
	initialTime = time(NULL);
	
}

void UpdateWaitbar(float nValue){
	// update the waitbar
	
	mexCallMATLAB(0, output_array, 2, input_array, "waitbar");

	static float nPreviousValue = 0.0f;

			
	if( (int)(floor(nValue * 100.f)) - (int) (floor(nPreviousValue * 100.f)) > 0){
		currentTime = time(NULL);
		if((currentTime - initialTime) == 0){
			nPreviousValue = nValue;
			return;
		}
		
		int estimetedTimeInSeconds = 0;
		if(nValue > 0.0)
			estimetedTimeInSeconds =(int)( ((float)(currentTime - initialTime)  * (float)((1.f-nValue) * 100.f)) /(float)((nValue) * 100.f)) ;
		
		int hours = (estimetedTimeInSeconds) / (3600);
		int minutes = ((estimetedTimeInSeconds) / 60) % 60;
		int seconds = (estimetedTimeInSeconds) % 60;
		
		
		*percentDoublePtr = nValue;
		mxDestroyArray(title);
		sprintf(tempString, "In progress ... (Estimated time = %d H : %d m : %d s)",hours, minutes,seconds);
		title = mxCreateString(tempString );
		input_array[0] = percent;
		input_array[1] = hw;		
		input_array[2] = title;
		mexCallMATLAB(0, output_array, 3, input_array, "waitbar");
	}

	nPreviousValue = nValue;
}

void PrintInMatlab(const char *nStr){
	mexPrintf(nStr);
	mexEvalString("drawnow");
}

void CloseWaitbar(){
	// Close the waitbar
	input_array[0] = hw;
	mexCallMATLAB(0, output_array, 1, input_array, "close");
	mxDestroyArray(percent);
	mxDestroyArray(title);
}

void PrintHelp(){

	PrintInMatlab("\n\n *** JLinkageLibClusterizeMex v.1.0 *** Part of the SamantHa Project");
	PrintInMatlab("\n     Author roberto.toldo@univr.it - Vips Lab - Department of Computer Science - University of Verona(Italy) ");
	PrintInMatlab("\n     vpdetection Author Chen Feng - simbaforrest@gmail.com - University of Michigan");
	PrintInMatlab("\n ***********************************************************************");
	PrintInMatlab("\n Usage: [Labels, PS] = JLnkClusterize(Points, Models, InlierThreshold, ModelType, *KDTreeRange = -1*, *ExistingClusters = []*)");
	PrintInMatlab("\n Input:");
	PrintInMatlab("\n        Points - Input dataset (Dimension x NumberOfPoints)");
	PrintInMatlab("\n        Models - Hypotesis generated by the JLinkageLibRandomSamplerMex(Dimension x NumberOfModels)");
	PrintInMatlab("\n        InlierThreshold - maximum inlier distance point-model ");
	PrintInMatlab("\n        ModelType - type of models extracted. Currently the model supported are: 0 - Planes 1 - 2dLines 2 - Vanishing Points");
	PrintInMatlab("\n        KDTreeRange(facultative) - Select the number of neighboards to use with in the agglomerative clustering stage. ( if <= 0 all the points are used; n^2 complexity)");
	PrintInMatlab("\n        ExistingClusters(facultative) - Already existing clusters, Logical Matrix containing Pts X NCluster");
	PrintInMatlab("\n Output:");
	PrintInMatlab("\n        Labels - Belonging cluster for each point");
	PrintInMatlab("\n        PS - Preference set of resulting clusters");
	PrintInMatlab("\n");
	
}

enum MODELTYPE{
	MT_PLANE,
	MT_LINE,
	MT_VP, //FC
	MT_SIZE
};


double *mTempMPointer = NULL;
//// Input arguments
// Arg 0, points
std::vector<std::vector<float> *> *mDataPoints;
// Arg 1, Models
std::vector<std::vector<float> *> *mModels;
// Arg 2, InliersThreshold
float mInlierThreshold;
// Arg 3, type of model: 0 - Planes 1 - 2dLines
MODELTYPE mModelType;
// ----- facultatives
// Arg 4, Select the KNN number of neighboards that can be merged. ( = 0 if all the points are used; n^2 complexity)
int mKDTreeRange = -1;
// Arg 5, Already existing clusters, Logical Matrix containing Pts X NCluster");
std::vector<std::vector<unsigned int> > mExistingClusters;

// Function pointers
std::vector<float>  *(*mGetFunction)(const std::vector<sPt *> &nDataPtXMss, const std::vector<unsigned int>  &nSelectedPts);
float (*mDistanceFunction)(const std::vector<float>  &nModel, const std::vector<float>  &nDataPt);


//// Output arguments
// Arg 0, NPoints x NSample logical preference set matrix
bool *mPSMatrix;

void mexFunction( int nlhs, mxArray *plhs[], 
		  int nrhs, const mxArray*prhs[] ) 
{ 
	/* retrive arguments */
	if( nrhs < 3 ){
		PrintHelp();
		mexErrMsgTxt("At least 3 arguments are required"); 
	}
	if( nlhs < 1 ){
		PrintHelp();
		mexErrMsgTxt("At least 1 output is required."); 
	}
	// arg2 : InlierThreshold
	mTempMPointer = mxGetPr(prhs[2]);
	mInlierThreshold = (float) *mTempMPointer;
	if(mInlierThreshold <= 0.0f)
		mexErrMsgTxt("Invalid InlierThreshold");

	// arg3 : modelType;
	mTempMPointer = mxGetPr(prhs[3]);
	mModelType = (MODELTYPE)(unsigned int) *mTempMPointer;
	switch(mModelType){
		case MT_PLANE: 
			mGetFunction = GetFunction_Plane;
			mDistanceFunction = DistanceFunction_Plane;
			break;
		case MT_LINE: 
			mGetFunction = GetFunction_Line;
			mDistanceFunction = DistanceFunction_Line;
			break;
		case MT_VP://FC
			mGetFunction = GetFunction_VP;
			mDistanceFunction = DistanceFunction_VP;
			break;
		default: mexErrMsgTxt("Invalid model type"); break;
	}

	
	// Arg 0, data points	
	mTempMPointer = mxGetPr(prhs[0]);
	
	if(mxGetN(prhs[0]) < mxGetM(prhs[0]))
		mexErrMsgTxt("Invalid DataPoints");
	
	// Build mDataPointVector
	mDataPoints = new std::vector< std::vector<float> *>(mxGetN(prhs[0]));
	for(unsigned int i=0; i<mxGetN(prhs[0]); i++){
		(*mDataPoints)[i] = new std::vector<float>(mxGetM(prhs[0]));
		for(unsigned int j=0; j<mxGetM(prhs[0]) ; j++)
			(*(*mDataPoints)[i])[j] = (float)mTempMPointer[(i*mxGetM(prhs[0])) + j];
	}
	
	// Arg 1, models
	mTempMPointer = mxGetPr(prhs[1]);
	
	if(mxGetN(prhs[1]) < mxGetM(prhs[1]))
		mexErrMsgTxt("Invalid Models");

	// Build mModels
	mModels = new std::vector< std::vector<float> *>(mxGetN(prhs[1]));
	for(unsigned int i=0; i<mxGetN(prhs[1]); i++){
		(*mModels)[i] = new std::vector<float>(mxGetM(prhs[1]));
		for(unsigned int j=0; j<mxGetM(prhs[1]) ; j++)
			(*(*mModels)[i])[j] = (float)mTempMPointer[(i*mxGetM(prhs[1])) + j];
	}

	// Arg4: KDTree Range
	if(nrhs > 4 && mxGetN(prhs[4]) > 0 && mxGetM(prhs[4]) > 0){
		mTempMPointer = mxGetPr(prhs[4]);
		mKDTreeRange = (int)*mTempMPointer;
	}

	// Arg5: already existing clusters
	mExistingClusters.clear();
	if(nrhs > 5 && mxGetN(prhs[5]) > 0 && mxGetM(prhs[5]) > 0){
		if(!mxIsLogical(prhs[5]) || mxGetM(prhs[5]) != mDataPoints->size())	
			mexErrMsgTxt("Invalid Existing cluster matrix");

		bool *mTempMPointerBool = (bool *)mxGetPr(prhs[5]);
		for(int j=0; j < (int)mxGetN(prhs[5]); ++j){
			std::vector<unsigned int> tCluster;
			for(int i=0; i < (int)mDataPoints->size(); ++i)
				if(mTempMPointerBool[i + j * mDataPoints->size()])
					tCluster.push_back(i);
			mExistingClusters.push_back(tCluster);
		}
	}
	

	PrintInMatlab("Initializing Data... \n");
	// Compute the jLinkage Clusterization
	JLinkage mJLinkage(mDistanceFunction, mInlierThreshold, mModels->size(), true, ((*mDataPoints)[0])->size(), mKDTreeRange);

	std::vector<const sPtLnk *> mPointMap(mDataPoints->size());	
		
	std::list<sClLnk *> mClustersList;
	
	PrintInMatlab("\tLoading Models \n");
	unsigned int counter = 0;
	InitializeWaitbar();
	for(unsigned int nModelCount = 0; nModelCount < mModels->size(); nModelCount++){
		mJLinkage.AddModel(((*mModels)[nModelCount]));
		++counter;
		UpdateWaitbar((float)counter/(float)mModels->size());
	}
	CloseWaitbar();
	
	
	PrintInMatlab("\tLoading Points \n");
	counter = 0;
	InitializeWaitbar();
	for(std::vector<std::vector<float> *>::iterator iterPts = mDataPoints->begin(); iterPts != mDataPoints->end(); ++iterPts ){
		mPointMap[counter] = mJLinkage.AddPoint(*iterPts);
		++counter;
		UpdateWaitbar((float)counter/(float)mDataPoints->size());
	}
	CloseWaitbar();

	if(mExistingClusters.size() > 0){
		PrintInMatlab("\tLoading Existing Models \n");
		for(int i=0; i < (int)mExistingClusters.size(); ++i){
			if(!mJLinkage.ManualClusterMerge(mExistingClusters[i]))
				mexErrMsgTxt("Invalid Existing cluster matrix");
		}
	}

	PrintInMatlab("J-Clusterizing \n");
	InitializeWaitbar();

	mClustersList = mJLinkage.DoJLClusterization(UpdateWaitbar);
	CloseWaitbar();
	unsigned int counterCl = 1;
	
	
	// Write output
	plhs[0] = mxCreateDoubleMatrix(1,mDataPoints->size(), mxREAL);
	double *mTempUintPointer = (double *)mxGetPr(plhs[0]);
	
	for(std::list<sClLnk *>::iterator iterCl = mClustersList.begin(); iterCl != mClustersList.end(); ++iterCl){
		for(std::list<sPtLnk *>::iterator iterPt = (*iterCl)->mBelongingPts.begin(); iterPt != (*iterCl)->mBelongingPts.end(); ++iterPt){
			unsigned int counterPt = 0;
			for(std::vector<const sPtLnk *>::iterator iterPtIdx = mPointMap.begin(); iterPtIdx != mPointMap.end(); ++iterPtIdx){
				if((*iterPt) == (*iterPtIdx)){
					mTempUintPointer[counterPt] = counterCl;
					break;
				}
				++counterPt;
			}
		}
		++counterCl;
	}

	// Write also resulting PS matrix if requested
	if( nlhs > 1 ){
		int Nclusters = mClustersList.size();
		plhs[1] = mxCreateLogicalMatrix(Nclusters, mModels->size()); 
		bool *mTempMPointerBool = (bool *)mxGetPr(plhs[1]);

		counterCl = 0;
		for(std::list<sClLnk *>::iterator iterCl = mClustersList.begin(); iterCl != mClustersList.end(); ++iterCl){
			for(unsigned int i = 0; i < mModels->size(); i++)
				mTempMPointerBool[counterCl +  Nclusters * i] = (*iterCl)->mPreferenceSet[i];
			++counterCl;
		}
	}

	// Clean up
	for(unsigned int i=0; i<mModels->size(); i++)
		delete (*mModels)[i];
	delete mModels;

	for(unsigned int i=0; i<mDataPoints->size(); i++)
		delete (*mDataPoints)[i];
	delete mDataPoints;

    return;

}


