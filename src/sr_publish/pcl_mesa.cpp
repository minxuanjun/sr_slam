#include "pcl/point_cloud.h"
#include "pcl/point_types.h"
#include "pcl/visualization/cloud_viewer.h"
#include "pcl/io/pcd_io.h"

#include "libMesaSR.h"

#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include <vector>
#include <typeinfo>
#include <ros/ros.h>

#include "std_msgs/UInt8MultiArray.h"
#include "SR_interface.h"
#include "cam_model.h"
#include "plane_extract.h"

using namespace std; 

typedef pcl::PointXYZ point_type; 
typedef pcl::PointCloud<point_type> cloud_type; 
typedef typename cloud_type::Ptr  cloudPtr; 

#define S2F 0.001

void showCam(SRCAM);

void testBase(SRCAM);

void showCloud(cloudPtr&);

void fromArray2PC(std_msgs::UInt8MultiArray& array, cloudPtr& pc );

template<typename T, typename T2>
void genCloud(cloudPtr&, int n, T* x, T* y, T2* z);

void openSR(SRCAM& srCam)
{
  SR_OpenETH(&srCam, "192.168.0.11"); // open camera 
  SR_SetIntegrationTime(srCam, 30); 
  SR_SetMode(srCam, AM_SW_TRIGGER | AM_CONF_MAP | AM_MEDIAN | AM_COR_FIX_PTRN | AM_SW_ANF | AM_CONV_GRAY); 
}

void runTest();
void testCam();
void testFile();
void testFile2();
void testTCP();


int main(int argc, char* argv[])
{
  ros::init(argc, argv, "pcl_show");
  ros::NodeHandle n;
  
  ros::NodeHandle nh_p("~"); 
  nh_p.setParam("sr_new_file_version", true); // new data version  
 
  if(argc >= 2)
  {
    if(argc == 3)
    {
      if(!strcmp(argv[2], "old"))
        nh_p.setParam("sr_new_file_version", false); // new data version  
    }

    if(!strcmp(argv[1], "tcp"))
    {
        cout<<"testTCP()"<<endl;
        testTCP();
    }else if(!strcmp(argv[1], "cam"))
    {
        cout<<"testCam()"<<endl;
        testCam();
    }else
    {
        cout<<"testFile()"<<endl;
        testFile();
    }

  }else
  {
    cout<<"testFile()"<<endl;
    testFile();
  }
  // testTCP();

  // testFile2();
  // SRCAM srCam;
  // openSR(srCam);
 
  // SR_OpenETH(&srCam, "192.168.0.11"); // open camera 
  // SR_SetIntegrationTime(srCam, 30); 
  // SR_SetMode(srCam, AM_SW_TRIGGER | AM_CONF_MAP | AM_MEDIAN | AM_COR_FIX_PTRN | AM_SW_ANF | AM_CONV_GRAY); 

  // SR_Acquire(srCam); 
  // SR_Close(srCam);
  // SR_SetMode(srCam, AM_SW_TRIGGER | AM_CONF_MAP);
  
  /*
  openSR(srCam);
  testBase(srCam); 
  showCam(srCam);
  SR_Close(srCam);
  */

  return 0;
}

void testTCP()
{
  ros::NodeHandle nh_p("~"); 
  nh_p.setParam("sr_source", "SR_TCP");
  // nh_p.setParam("sr_data_file_dir", "/home/davidz/work/EmbMess/mesa/pcl_mesa/build/bin/sr_data"); 
  
  nh_p.setParam("sr_server_ip", "192.168.100.2");
  // nh_p.setParam("sr_server_ip", "192.168.0.100");
  // nh_p.setParam("sr_end_frame", 50); 
  // nh_p.setParam("sr_new_file_version", true); // new data version  
  // nh_p.setParam("sr_data_file_dir", "/home/davidz/work/data/SwissRanger4000/try");

  // 1, generate sr data; 
  CSRInterface sr_instance; 
  std_msgs::UInt8MultiArray sr_array;
  
  if(!sr_instance.open())
  {
    return ;
  }

  // PCL point cloud 
  pcl::PointCloud<pcl::PointXYZ>::Ptr 
    cloud(new pcl::PointCloud<point_type>(144, 176));
  

  // visualization 
  pcl::visualization::CloudViewer viewer("MesaSR PCL Viewer"); 
 
  // display it 
  while(!viewer.wasStopped())
  {
    if(sr_instance.get(sr_array))
    {
      fromArray2PC(sr_array, cloud);
      

      viewer.showCloud(cloud);
    }
    usleep(30000); // sleep 30 ms 
  }

  /*
  for(int i=0;i<50; )
  {
    if(!sr_instance.get(sr_array))
    {
      usleep(10000); // sleep 10ms 
      continue;
    }
    fromArray2PC(sr_array, cloud);
    cout<<"pcl_mesa.cpp: show the "<<i+1<<" cloud"<<endl;
    
    stringstream ss; 
    ss<<"./tmp_pcd/cloud_"<<i+1<<".pcd"; 
    pcl::io::savePCDFile(ss.str(), *cloud);

    i++;
  }
  */
  // disconnet TCP 
  sr_instance.close();
  return;
}

void fromArray2PC(std_msgs::UInt8MultiArray& array, cloudPtr& pc )
{
  ros::NodeHandle nh_p("~"); 
  bool b_new_file_model; 
  nh_p.param("sr_new_file_version", b_new_file_model, false);
  // nh_p.param("sr_use_camera_model", b_use_cam_model, false);
  
  // pointer to the msg array address
  int total = 176*144; 
  unsigned char* pSrc = array.data.data();
  if(pc->points.size() != total) pc->points.resize(total);
    
  if(b_new_file_model)
  {
    // static CamModel cam_model(223.9758, 226.7442, 89.361, 75.8112);     
    static CamModel cam_model(250.5773, 250.5773, 90, 70, -0.8466, 0.5370);
    static vector<unsigned short> dis_(total, 0); 
    memcpy(dis_.data(), pSrc, total*sizeof(unsigned short)); // get distance array

    unsigned short* pD = (unsigned short*)(dis_.data());
    int k;
    float z;  
    double ox,oy,oz;
    int sr_rows = 144; 
    int sr_cols = 176;
    for(int i=0; i < sr_rows; i++)
    {
      for(int j=0; j <sr_cols; j++)
      {
        k = j + i*sr_cols;
        z = *pD*0.001; // convert mm to m
        pcl::PointXYZ & pt = pc->points[k];

        if(z <= 0.01 || z>= 9.5)
        {
          pt.x = pt.y = pt.z = 0; 
        }else
        {
          z = z + 0.01; // Z is in the swiss ranger reference, z = Z + shift, to rectified camera reference
          cam_model.convertUVZ2XYZ(j+0.5, i+0.5, z, ox, oy, oz); 
          pt.x = -ox;  pt.y = -oy; pt.z = oz - 0.01 ; // 1cm shift, transfer to the swiss ranger reference
          
          // pt.x = oz - 0.01; 
          // pt.y = -ox; 
          // pt.z = -oy;
        }
        ++pD; 
      }
    }

  }else
  {
    // static unsigned int total_all = total*(sizeof(unsigned short)*1 + sizeof(float)*3);
    static unsigned int total_all = total*(sizeof(unsigned char)*1 + sizeof(float)*3);
    // int img_offset = total*sizeof(unsigned short);
    int img_offset = total*sizeof(unsigned char);
    int pt_offset = total*sizeof(float);
    vector<float> x(total, 0); vector<float> y(total, 0); vector<float> z(total, 0); 
    memcpy(x.data(), pSrc + img_offset, pt_offset);
    memcpy(y.data(), pSrc + img_offset + pt_offset, pt_offset); 
    memcpy(z.data(), pSrc + img_offset + pt_offset*2, pt_offset); 
    for(int i=0; i<total; i++)
    {
      point_type & pt = pc->points[i]; 
      if(z[i] <= 0.01 || z[i] >= 9.5) 
      {
        pt.x = pt.y = pt.z = 0; 
      }else{
        pt.x = x[i]; pt.y = y[i]; pt.z = z[i];
      }
    }
  }
}

void testFile2()
{
  ros::NodeHandle nh_p("~"); 
  nh_p.setParam("sr_source", "SR_FILE"); 
  nh_p.setParam("sr_end_frame", 10); 
  // nh_p.setParam("sr_new_file_version", false); // test old file version  

  // 1, get file from sr_reader
  CSReader reader;
  reader.loadAllData();
  sr_data data = reader.get_frame(1); 
  // sr_data data; 
  /*
  {
    stringstream ss; 
    ss<<"/home/davidz/work/data/SwissRanger4000/try/d1_0001.bdat";
    FILE* fid = fopen(ss.str().c_str(), "rb"); 
    fread(&data.z_[0], sizeof(SR_TYPE), SR_SIZE, fid);
    fread(&data.x_[0], sizeof(SR_TYPE), SR_SIZE, fid); 
    fread(&data.y_[0], sizeof(SR_TYPE), SR_SIZE, fid); 
    fread(&data.intensity_[0], sizeof(SR_IMG_TYPE), SR_SIZE, fid);
    fread(&data.c_[0], sizeof(SR_IMG_TYPE), SR_SIZE, fid);
  }*/
  // 2, fill in the point cloud 
  cloudPtr cloud(new cloud_type); 
  int total = 176*144; 
  cloud->points.resize(total); 
  for(int i=0; i<total; i++)
  {
    point_type& pt = cloud->points[i]; 
    pt.x = data.x_[i];
    pt.y = data.y_[i]; 
    pt.z = data.z_[i];
  }
  // 3, show it
  pcl::visualization::CloudViewer viewer("MesaSR PCL Viewer"); 
  while(!viewer.wasStopped())
  {
    viewer.showCloud(cloud);
    usleep(30000); // sleep 30 ms 
  } 
  // 4, dump it into file 
  cloud->width = total; 
  cloud->height = 1;
  pcl::io::savePCDFile("tmp.pcd", *cloud);
}

void runTest()
{
  // 1, generate sr data; 
  CSRInterface sr_instance; 
  std_msgs::UInt8MultiArray sr_array;
  
  if(!sr_instance.open())
  {
    return ;
  }
  
  // weather to extract floor 
  bool b_show_floor_only_ ;
  ros::NodeHandle nh_p("~"); 
  nh_p.param("show_floor_only", b_show_floor_only_, true);

  // PCL point cloud 
  pcl::PointCloud<pcl::PointXYZ>::Ptr 
    cloud(new pcl::PointCloud<point_type>(144, 176));
  
  pcl::PointCloud<pcl::PointXYZ>::Ptr floor(new pcl::PointCloud<pcl::PointXYZ>);

  // visualization 
  pcl::visualization::CloudViewer viewer("MesaSR PCL Viewer"); 
  while(!viewer.wasStopped())
  {
    if(!sr_instance.get(sr_array)) 
    {
      printf("pcl_mesa.cpp: failed to get camera data!\n");
      break;
    }
    // ROS_INFO("pcl_mesa.cpp: get array size %d", sr_array.data.size());
    // cout<<"pcl_mesa.cpp: what' wrong with size: "<<sr_array.data.size()<<endl;
    fromArray2PC(sr_array, cloud);
    
    if(b_show_floor_only_) // we will extract floor, and show it
    {
      bool ret = ((CPlaneExtract<pcl::PointXYZ>*)(0))->extractFloor(cloud, floor); 
      if(ret)
      {
        viewer.showCloud(floor);
      }else
        viewer.showCloud(cloud);
    }
    else
      viewer.showCloud(cloud);
    usleep(30000); // sleep 30 ms 
  }
  /*
  for(int i=0;i<50; i++)
  {
    sr_instance.get(sr_array); 
    fromArray2PC(sr_array, cloud);
    cout<<"pcl_mesa.cpp: show the "<<i+1<<" cloud"<<endl;
  }
  */
}

void testCam()
{
  ros::NodeHandle nh_p("~"); 

  // set camera parameters
  nh_p.setParam("sr_source", "SR_CAM");
  // nh_p.setParam("sr_new_file_version", true); // test old file version  
  nh_p.setParam("sr_cam_ip", "192.168.1.4"); 

  runTest();
  return ;
}

void testFile()
{
  ros::NodeHandle nh_p("~"); 
  nh_p.setParam("sr_source", "SR_FILE");
  // nh_p.setParam("sr_data_file_dir", "/home/davidz/work/EmbMess/mesa/pcl_mesa/build/bin/sr_data"); 
  // nh_p.setParam("sr_data_file_dir", "/home/davidz/work/data/SwissRanger4000/try");
  // nh_p.setParam("sr_end_frame", 50); 
  // nh_p.setParam("sr_new_file_version", false); // test old file version  

  // nh_p.setParam("sr_data_file_dir", "/home/davidz/work/data/SwissRanger4000/try");

  // set camera parameters
  // nh_p.setParam("sr_source", "SR_CAM");
  // nh_p.setParam("sr_new_file_version", true); // test old file version  
  // nh_p.setParam("sr_cam_ip", "192.168.1.4"); 

  runTest(); 
  return ;
}

void testBase(SRCAM cam)
{
  SR_Acquire(cam); 
  
  // 1, set buffer 
  int rows = SR_GetRows(cam); 
  int cols = SR_GetCols(cam); 
  int total = rows*cols; 
  
  int short_s = sizeof(short); 
  int ushort_s = sizeof(unsigned short);
  int float_s = sizeof(float);
  vector<short> xs16(total, 0); vector<short> ys16(total, 0); vector<unsigned short> zs16(total, 0); 
  vector<float> xf32(total, 0); vector<float> yf32(total, 0); vector<float> zf32(total, 0); 
  vector<unsigned char> xc8(total, 0); vector<unsigned char> yc8(total, 0); vector<unsigned short> iDst(total, 0); 
  WORD* dst; 
  // 2, get XYZ using different functions 
  dst = (WORD*)SR_GetImage(cam, 0); // distance map 

  int k = 0;
  for(int j=0; j<rows; j++)
  {
    for(int i=0; i<cols; i++)
      {
        yc8[k] = j; 
        xc8[k] = i;
        iDst[k] = dst[j*cols + i];
        k++;
      }
  }
  
  // 3, get data 
  SR_CoordTrfUint16(cam, &xs16[0], &ys16[0], &zs16[0], short_s, short_s, ushort_s); 
  SR_CoordTrfFlt(cam, &xf32[0], &yf32[0], &zf32[0], float_s, float_s, float_s); 

  // 4, generate cloud 
  cloudPtr pc(new cloud_type);
  genCloud<short, unsigned short>(pc, total, &xs16[0], &ys16[0], &zs16[0]); 
  cout<<"pcl_mesa.cpp: show cloud short!"<<endl;
  showCloud(pc); 

  // 5, using the other function
  SR_CoordTrfPntUint16(cam, &xc8[0], &yc8[0], &iDst[0], &xs16[0], &ys16[0], &zs16[0], total); 
  genCloud<short, unsigned short>(pc, total, &xs16[0], &ys16[0], &zs16[0]); 
  cout<<"pcl_mesa.cpp: show cloud short using dis function!"<<endl;
  showCloud(pc); 

  // similarly 
  genCloud<float, float>(pc, total, &xf32[0], &yf32[0], &zf32[0]);
  cout<<"pcl_mesa.cpp: show cloud float!"<<endl;
  showCloud(pc);

  SR_CoordTrfPntFlt(cam, &xc8[0], &yc8[0], &iDst[0], &xf32[0], &yf32[0], &zf32[0], total); 
  genCloud<float, float>(pc, total, &xf32[0], &yf32[0], &zf32[0]);
  cout<<"pcl_mesa.cpp: show cloud float using dis function!"<<endl;
  showCloud(pc);

  return;
}

template<typename T, typename T2>
void genCloud(cloudPtr& pc, int n, T* x, T* y, T2* z)
{
  pc->points.resize(n);
  T* px = x; T* py = y; T2* pz = z; 
  for(int i=0; i<n; i++)
  {
    point_type & pt = pc->points[i]; 
    if(typeid(T).name() == typeid(short).name())
    {
      pt.x = *px*S2F; 
      pt.y = *py*S2F; 
      pt.z = *pz*S2F;
    }else
    {
      pt.x = *px; 
      pt.y = *py;
      pt.z = *pz;
    }
    ++px; ++py; ++pz;
  }
}


void showCloud(cloudPtr& pc)
{
  // visualization 
  pcl::visualization::CloudViewer viewer("MesaSR PCL Viewer"); 
  while(!viewer.wasStopped())
  {
    viewer.showCloud(pc);
    usleep(30000); // sleep 30 ms 
  }
}

void showCam(SRCAM srCam)
{
  int rows = SR_GetRows(srCam); 
  int cols = SR_GetCols(srCam); 

  // PCL point cloud 
  pcl::PointCloud<pcl::PointXYZ>::Ptr 
    cloud(new pcl::PointCloud<point_type>(rows, cols));

  float* ptrXYZ = (float*)(&cloud->front()); 
  int s = sizeof(point_type); 
  
  // visualization 
  pcl::visualization::CloudViewer viewer("MesaSR PCL Viewer"); 
  
  while(!viewer.wasStopped())
  {
    SR_Acquire(srCam);
    SR_CoordTrfFlt(srCam, &ptrXYZ[0], &ptrXYZ[1], &ptrXYZ[2], s, s, s); 
    viewer.showCloud(cloud);
    usleep(30000); // sleep 30 ms 
  }

}

