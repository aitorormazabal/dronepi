

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Helper.h"
#include <unistd.h>
#include <math.h>
#include "I2Cdev.h"
#include  "MPU6050_9Axis_MotionApps41.h"
#include "IMU.h"
#include "FC.h"
#include <stdlib.h>

IMU& IMU::instance(){
    static IMU instance;
    return instance;
}

void IMU::Init(){
    printf("Initializing I2C devices...\n");
    mpu.initialize();

    // verify connection
    printf("Testing device connections...\n");
    printf(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed\n");
    connexion=mpu.testConnection();

    if (!connexion)
      ;

    
    printf("Initializing DMP...\n");
    devStatus = mpu.dmpInitialize();

    if (devStatus==0)
    {
      printf("Enabling DMP\n");
      mpu.setDMPEnabled(true);
      packetSize = mpu.dmpGetFIFOPacketSize();
    }
    else
    {
      printf("DMP Initialization failed %d \n",devStatus);
    }
    gyroScale=16.4F;
    anglesDMP[0]=0;
    anglesDMP[2]=0;
    anglesDMP[1]=0;
    anglesCF[0]=0;
    anglesCF[1]=0;
    anglesCF[2]=0;
    calAngles[0]=0;
    calAngles[1]=0;
    calAngles[2]=0;
    packetFlag=false;
}

void IMU::RotateIMU(){
  float tmp=rawAcc[0];
  rawAcc[0]=rawAcc[1];
  rawAcc[1]=tmp;
  rawAcc[2]*=-1;

  tmp=rawGyro[0];
  rawGyro[0]=rawGyro[1];
  rawGyro[1]=tmp;
  rawGyro[2]*=-1;
}
float IMU::CorrectedTarget(float currTarget)
{
  float diff=currTarget-angle(2);
  while (diff<-180)
    diff+=360;
  while (diff>180)
    diff-=360;
  return angle(2)+diff;
}
void IMU::ReadDMP()
{
   int fifoCount;
   uint8_t fifoBuffer[64];
  fifoCount = mpu.getFIFOCount();
  if (fifoCount>=3*packetSize)
  {
  mpu.resetFIFO();
  printf("\n\n\n\n\n\n\n------------------------------------------------------------\n\n\n\n\n\n\nFIFO OVERFLOW\n\n\n\n\n\n\n-----------------------------------------------\n\n\n\n\n\n\n\n\n\n\n\n");
  }
  if (fifoCount >= packetSize)
  {
    Quaternion q;
    VectorFloat gravity;  
    mpu.getFIFOBytes(fifoBuffer, packetSize);
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    
    VectorFloat g2=gravity;
    g2.normalize();
    mpu.dmpGetYawPitchRoll(anglesDMP, &q, &gravity);
    float y=anglesDMP[0];
    float r=anglesDMP[1];
    float p=-anglesDMP[2];
    //switch from  yrp to rpy order and from radian to degree
    anglesDMP[0]=r*180/M_PI;
    anglesDMP[1]=p*180/M_PI;
    anglesDMP[2]=y*180/M_PI;
    
  }
}
void IMU::Update()
{
   mpu.getMotion6(&rawAcc[0], &rawAcc[1], &rawAcc[2], &rawGyro[0], &rawGyro[1], &rawGyro[2]);
   ReadDMP();
   RotateIMU();

   gyroData[0]=rawGyro[0]/gyroScale;
   gyroData[1]=rawGyro[1]/gyroScale;
   gyroData[2]=rawGyro[2]/gyroScale;
   CalculateEulerRates();

   double xSQ=rawAcc[0]*rawAcc[0];
   double ySQ=rawAcc[1]*rawAcc[1];
   double zSQ=rawAcc[2]*rawAcc[2];

   double normXZ=sqrt(xSQ+zSQ);
   double normYZ=sqrt(ySQ+zSQ);

   double pitchAccel=-atan2(rawAcc[0],normYZ)*180/3.1415f;
   double rollAccel=atan2(rawAcc[1],normXZ)*180/3.1415f;
   anglesCF[0]=0.95*(anglesCF[0]+eulerRates[0]*FC::LOOP_TIME_S)+0.05*rollAccel;
   anglesCF[1]=0.95*(anglesCF[1]+eulerRates[1]*FC::LOOP_TIME_S)+0.05*pitchAccel;

   
}
void IMU::CalculateEulerRates(){//Transforms angular rates read by gyro to euler angle rates
  if (cos(anglesCF[1])==0)
    return;
  float rRad=anglesCF[0]*3.1415f/180.0f;
  float pRad=anglesCF[1]*3.1415f/180.0f;
  float yRad=anglesCF[2]*3.1415f/180.0f;
  eulerRates[0]=gyroData[0] + gyroData[1]*sin(rRad)*tan(pRad) + gyroData[2]*cos(rRad)*tan(pRad);
  eulerRates[1]=gyroData[1]*cos(rRad) - gyroData[2]*sin(rRad);
  eulerRates[2]=gyroData[1]*sin(rRad)/cos(pRad) + gyroData[2]*cos(rRad)/cos(pRad);
  /*eulerRates[0]=gyroData[0];
  eulerRates[1]=gyroData[1];
  eulerRates[2]=gyroData[2];*/
}
void IMU::SetLevel()
{
  for (int i=0;i<3;i++)
    calAngles[i]=anglesDMP[i];
}
float IMU::rate(int axis)
{
  return gyroData[axis];
}
float IMU::angle(int axis)
{
  return anglesDMP[axis]-calAngles[axis];
}
void IMU::PrintState(){
  printf("X:%7.2f Y:%7.2f  Z:%7.2f  RX:%7.2f  RY:%7.2f  RZ:%7.2f  DX:%7.2f  DY:%7.2f \n",angle(0),angle(1),angle(2),gyroData[0],gyroData[1],gyroData[2],eulerRates[0]-gyroData[0],eulerRates[1]-gyroData[1]);
}