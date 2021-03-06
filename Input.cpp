#include "Input.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/kd.h>
#include "FC.h"
#include <unistd.h>
#include "Helper.h"
#include <time.h>

Input& Input::instance(){
    static Input instance;
    return instance;
  }
 void Input::Init()
{   
  for (int i=0;i<3;i++)
  {
    targetRate[i]=0;
    targetAngle[i]=0;
    cameraPoseT[i]=0;
  }
  throttle=0;
  
  commsFD=socket(AF_UNIX,SOCK_DGRAM,0);
  if (commsFD<0)
    printf("Error creating socket\n");
  int res=fcntl(commsFD,F_SETFL,O_NONBLOCK);
  if (res<0)
    printf("Error setting socket to nonblocking mode\n");
  struct sockaddr_un addr;
  memset(&addr,0,sizeof(addr));
  addr.sun_family=AF_UNIX;
  strncpy(addr.sun_path, "comms",sizeof(addr.sun_path)-1);
  unlink("comms");
  res=bind(commsFD, (struct sockaddr *)&addr,sizeof(addr));
  if (res<0)
    printf("Error binding socket to address\n");
}

void Input::Update()
{
  char code;
  while (true)
  {
    char buf[10];
	  int res=recv(commsFD,buf,10,0);
    if (res<=0){
      break;
    }
    code=buf[0];
    buf[res]='\0';
    int readData[4];
    for (int i=0;i<4;i++)
    {
      readData[i]=buf[i+1];
    }

    if (code=='N')
    {//Data comes in order: throttle pitch roll yaw
      int throttleData=constrain((readData[0]&127),0,100);
      int pitchSign=(readData[1]&128);
      int pitchData=constrain((readData[1]&127),0,200);
      if (pitchSign>0)
        pitchData=-pitchData;
      int rollSign=readData[2]&128;
      int rollData=constrain((readData[2]&127),0,200);
      if (rollSign>0)
        rollData=-rollData;
      int yawSign=(readData[3]&128);
      int yawData=constrain((readData[3]&127),0,200);
      if (yawSign>0)
        yawData=-yawData;
      WriteInputs(pitchData, rollData, yawData, throttleData);
    }
    else if (code=='S')//S for state
    {
      char state=readData[0];
      if (state=='A')//Angle
        state=FC::STATE_ARMED_ANGLE;
      else if (state=='R')//Rate
        state=FC::STATE_ARMED_RATE;
      else if (state=='U')//Unarm
        state=FC::STATE_UNARMED;
      FC::instance().ChangeState(state);
    }
    else if (code=='P')//P for PItch
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      if (P==30&&I==30&&D==30)
        FC::instance().Unarm();
      else
        FC::instance().setRatePIDs(P/10,I/10,D/10,1);
      
    }
    else if (code=='R')//R for roll
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      FC::instance().setRatePIDs(P/10,I/10,D/10,0);
      
    }
    else if (code=='y')//yaw
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      FC::instance().setRatePIDs(P/10,I/10,D/10,2);
    }
    else if (code=='X')//X for angle PID
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      FC::instance().setAnglePIDs(P/10,I/10,D/10,0);
    }
    else if (code=='Y')//X for angle PID
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      FC::instance().setAnglePIDs(P/10,I/10,D/10,1);
    }
    else if (code=='Z')//angle yaw
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      FC::instance().setAnglePIDs(P/10,I/10,D/10,2);
    }
    else if (code=='T')//throttle
    {
      float P=readData[0]&127;
      float I=readData[1]&127;
      float D=readData[2]&127;
      FC::instance().setAltitudePID(P/10,I/10,D/10);
    }
    else if (code=='C')//Translation data from camera pose estimation
    {
      char signs=buf[1];//First three bits of this byte represent signs of the coordinates, 0 if negative, 1 if positive
      int xSign=signs & 1;
      xSign=2*xSign-1;//After this xSign is -1 if x coordinate is negative, 1 if it is positive
      int ySign=(signs >> 1) & 1;
      ySign=2*ySign-1;
      int zSign=(signs >> 2) & 1;
      zSign=2*zSign-1;

      //Next 6 bytes are the absolute value of x,y,z coordinates, multiplied by 1000. Two bytes for each, least significant byte first.
      char x1=buf[2];
      char x2=buf[3];
      char y1=buf[4];
      char y2=buf[5];
      char z1=buf[6];
      char z2=buf[7];
      cameraPoseT[0]=xSign * (x1+256*x2);
      cameraPoseT[0]/=1000;
      cameraPoseT[1]=ySign * (y1+256*y2);
      cameraPoseT[1]/=1000;
      cameraPoseT[2]=zSign * (z1+256*z2);
      cameraPoseT[2]/=1000;
    }
    FC::instance().ResetInputTimer();
  }
}

void Input::WriteInputs(int pitch, int roll, int yaw, int throttle)
{
  if (FC::instance().state()==FC::STATE_ARMED_RATE)
  {
    this->throttle=throttle;
    targetRate[0]=roll;
    targetRate[1]=pitch;
    targetRate[2]=yaw;
  }
  else if (FC::instance().state()==FC::STATE_ARMED_ANGLE)
  {
    this->throttle=throttle;
    targetAngle[0]=40*roll/100;
    targetAngle[1]=40*pitch/100;
    targetAngle[2]=40*yaw/100;
  }
  else if (FC::instance().state()==FC::STATE_UNARMED)
  {
    this->throttle=0;
    targetRate[0]=0;
    targetRate[1]=0;
    targetRate[2]=0;
  }
}
void Input::PrintState(){
  printf("Inputs AX:%f AY:%f AZ:%f RX:%f RY:%f RZ:%f T:%d CX:%f CY:%f CZ:%f\n",targetAngle[0],targetAngle[1],targetAngle[2],targetRate[0],targetRate[1],targetRate[2],throttle, cameraPoseT[0],cameraPoseT[1],cameraPoseT[2]);
}



