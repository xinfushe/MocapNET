#include "gestureRecognition.hpp"
#include "mocapnet.hpp"
#include "bvh.hpp"
#include "csv.hpp"


#define NORMAL   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */

int addToMotionHistory(struct PoseHistory * poseHistoryStorage,std::vector<float> pose)
{
    poseHistoryStorage->history.push_back(pose);
     if (poseHistoryStorage->history.size() > poseHistoryStorage->maxPoseHistory)
                {
                    poseHistoryStorage->history.erase(poseHistoryStorage->history.begin());
                }
    return 1;
}



int dumpMotionHistory(const char * filename,struct PoseHistory * poseHistoryStorage)
{
  return writeBVHFile(filename,0,poseHistoryStorage->history);  
}



int updateGestureActivity(std::vector<float> vecA,std::vector<float> vecB,std::vector<char> &active,float threshold)
{
  if (vecA.size()!=vecB.size())
  {
    fprintf(stderr,"updateGestureActivity cannot compare different sized vectors..\n");
    return 0;
  }
  
  if (vecA.size()!=active.size())
  {
    fprintf(stderr,"updateGestureActivity cannot compare without allocated active vector..\n");
    return 0;
  }


   //Always skip X pos, Y pos, Z pos , and rotation
   for (int i=6; i<vecA.size(); i++)
   {
     float difference=abs(vecA[i]-vecB[i]);
     if  (difference>threshold)  
           {
            active[i]=1;
           }
   }
 return 1;
}


int automaticallyObserveActiveJointsInGesture(struct RecordedGesture * gesture)
{
  if (gesture->gesture.size()==0)
  {
   fprintf(stderr,"Failed to automatically observe active joints\n");   
   return 0;   
  }
   
  std::vector<float> initialPose = gesture->gesture[0]; 
   
  int jointID=0;
  for (jointID=0; jointID<initialPose.size(); jointID++)
  { 
    gesture->usedJoints.push_back(0);  
  }
  
  for (jointID=0; jointID<initialPose.size(); jointID++)
  { 
     updateGestureActivity(
                            initialPose,
                            gesture->gesture[jointID],
                            gesture->usedJoints,
                            25
                           );
  }
   
 return 1;   
}

 
int loadGestures(struct GestureDatabase * gestureDB)
{
     unsigned  int gestureID;
     char gesturePath[512]={0};
     
     gestureDB->gestureChecksPerformed=0;
     
     for (gestureID=0; gestureID<hardcodedGestureNumber; gestureID++)
     {
         snprintf(gesturePath,512,"dataset/gestures/%s",hardcodedGestureName[gestureID]);
         fprintf(stderr,"Loading Gesture %u -> %s  : ",gestureID,gesturePath); 
         
         gestureDB->gesture[gestureID].gesture=loadBVHFileMotionFrames(gesturePath);
         //gestureDB->gesture[gestureID].bvhMotionGesture=loadBVHFile(gesturePath);
         
         //if (gestureDB->gesture[gestureID].bvhMotionGesture!=0)
         if (gestureDB->gesture[gestureID].gesture.size()>0)
         {
           gestureDB->gesture[gestureID].loaded=1;
           gestureDB->gesture[gestureID].gestureCallback=0;
           snprintf(gestureDB->gesture[gestureID].label,128,"%s",hardcodedGestureName[gestureID]);
           gestureDB->numberOfLoadedGestures +=1;
           
           //Populate
           //gestureDB->gesture[gestureID].gesture; 
           if (automaticallyObserveActiveJointsInGesture(&gestureDB->gesture[gestureID]))
           {
              for (int jointID=0; jointID<gestureDB->gesture[gestureID].usedJoints.size(); jointID++)
                  {  
                     if (gestureDB->gesture[gestureID].usedJoints[jointID])
                     {
                       fprintf(stderr,YELLOW "Joint %u/%s is going to be used\n" NORMAL,jointID,MocapNETOutputArrayNames[jointID]); 
                     }
                  }
           } else
           {
              fprintf(stderr,YELLOW "Failure observing active joints\n" NORMAL); 
           }
           
           fprintf(stderr,GREEN "Success , loaded %lu frames\n" NORMAL,gestureDB->gesture[gestureID].gesture.size()); 
         } else
         {
          fprintf(stderr,RED "Failure\n" NORMAL);              
         }
          
     }

 return  (gestureDB->numberOfLoadedGestures>0);
}




int areTwoBVHFramesCloseEnough(std::vector<float> vecA,std::vector<float> vecB,std::vector<char> active,float threshold)
{
  if (vecA.size()!=vecB.size())
  {
    fprintf(stderr,"areTwoBVHFramesCloseEnough cannot compare different sized vectors..\n");
    return 0;
  }

/*
  if (vecA.size()!=active.size())
  {
    fprintf(stderr,"areTwoBVHFramesCloseEnough cannot compare without active vectors..\n");
    return 0;
  }
*/
   
   //Always skip X pos, Y pos, Z pos , and rotation
   for (int i=6; i<vecA.size(); i++)
   {
     float difference=abs(vecA[i]-vecB[i]);
     
     if (active.size()==0)
     {
      //If there is no active vector consider all joints as active..
      if (difference>threshold)
      {
        return 0; 
      }
     } else
     {
        if (i>=active.size())
        {
          fprintf(stderr,RED "Failure comparing bvh frames due to short activeJoint vector\n" NORMAL);     
          return 0;  
        }  else
        {
          if ( (active[i]) && (difference>threshold) )
           {
            return 0; 
           }
        }
     }
   }


  return 1;  
}



int compareHistoryWithGesture(struct RecordedGesture * gesture ,struct PoseHistory * poseHistoryStorage,unsigned int timestamp,float percentageForDetection,float threshold)
{ 
  unsigned int matchingFrames=0;
  if (poseHistoryStorage->history.size()==0)
  {
    fprintf(stderr,"Requested comparison with empty history..\n");  
    return 0;  
  }
  
  if (gesture->lastActivation>timestamp)
  {
    fprintf(stderr,RED "compareHistoryWithGesture: inverted timestmaps, something is terribly wrong..\n" NORMAL);  
  } else
  {
    if (timestamp-gesture->lastActivation<20)
    {
       fprintf(stderr,YELLOW "gesture %s on cooldown..\n" NORMAL,gesture->label);  
       return 0; 
    }     
  }    
  
  
  
  
  if (poseHistoryStorage->history.size()>=gesture->gesture.size())
  {
     unsigned int historyStart=poseHistoryStorage->history.size() - gesture->gesture.size();
     unsigned int frameID=0,jointID=0;
     unsigned int jointNumber=poseHistoryStorage->history[0].size();
 
     for (frameID=0; frameID<gesture->gesture.size(); frameID++)
     {
        matchingFrames+=areTwoBVHFramesCloseEnough(gesture->gesture[frameID],poseHistoryStorage->history[frameID+historyStart],gesture->usedJoints,threshold);
     } 
     
     gesture->percentageComplete = (float) matchingFrames/gesture->gesture.size();
     
     if (100*gesture->percentageComplete >= percentageForDetection )
     {
       fprintf(stderr,GREEN);
       gesture->lastActivation=timestamp;
     }
     fprintf(stderr,"Gesture %s - %0.2f %% %u/%lu\n" NORMAL,gesture->label,gesture->percentageComplete*100,matchingFrames,gesture->gesture.size());
     
     //If we have more than 75% correct trigger..!
     return (100*gesture->percentageComplete >= percentageForDetection );
  }  
    
  return 0;  
}



int compareHistoryWithKnownGestures(struct GestureDatabase * gestureDB,struct PoseHistory * poseHistoryStorage,float percentageForDetection,float threshold)
{
  unsigned int gestureID=0; 
  gestureDB->gestureChecksPerformed+=1;
 
  for (gestureID=0; gestureID<gestureDB->numberOfLoadedGestures; gestureID++)
     {
       if (
           compareHistoryWithGesture(
                                      &gestureDB->gesture[gestureID],
                                      poseHistoryStorage,
                                      gestureDB->gestureChecksPerformed,
                                      percentageForDetection,
                                      threshold
                                    )
          )
       {
           return 1+gestureID;
       }   
     } 
  return 0;    
}



