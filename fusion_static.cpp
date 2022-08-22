#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

const double epsilon = 1e-12; //小于该数判断为0
static int ret;
static int fd_imu;
static int fd_gps;
static char IMU_ADDR = 0x50;
static int gps_data_size = 88;

#define BORD_IMU 9600
#define BORD_GPS 115200

static char accbyteSend[8] = {
    IMU_ADDR,
    0x03,
    0x00,
    0x34,
    0x00,
    0x0C,
    0X09,
    0X80};

// IMU加速度数据
double a[3], w[3], Angle[3];
// gps数据
vector<string> gpsData(3);

template <class Type>
Type stringToNum(const string &str)
{
  istringstream iss(str);
  Type num;
  iss >> num;
  return num;
}

vector<vector<double>> creatmatrix(int h, int l)
{
  vector<vector<double>> v;
  for (int i = 0; i < h; i++)
  {
    vector<double> v1(l, 0);
    v.push_back(v1);
  }
  return v;
}

//矩阵相加
vector<vector<double>> plusMartrix(vector<vector<double>> A, vector<vector<double>> B)
{
  int h = A.size();
  int l = A[0].size();
  vector<vector<double>> C;
  C = creatmatrix(h, l);

  for (int i = 0; i < h; i++)
  {
    for (int j = 0; j < l; j++)
    {
      C[i][j] = A[i][j] + B[i][j];
      if (abs(C[i][j]) < epsilon)
      {
        C[i][j] = 0.0;
      }
    }
  }
  return C;
}

//矩阵相乘
vector<vector<double>> multiply(vector<vector<double>> A, vector<vector<double>> B)
{
  int A_h = A.size();
  int A_l = A[0].size();
  int B_h = B.size();
  int B_l = B[0].size();
  if (A_l != B_h)
  {
    cout << "两矩阵维数无法相乘" << endl;
    exit(0);
  }
  vector<vector<double>> C = creatmatrix(A_h, B_l);
  for (int i = 0; i < A_h; i++)
  {
    for (int j = 0; j < B_l; j++)
    {
      C[i][j] = 0;
      for (int k = 0; k < A_l; k++)
      {
        C[i][j] += A[i][k] * B[k][j];
      }
      if (abs(C[i][j]) < epsilon)
      {
        C[i][j] = 0.0;
      }
      // cout<<C[i][j]<<"\t";
    }
    // cout<<endl;
  }
  return C;
}

//矩阵与数字相乘
vector<vector<double>> multiplyWithNumber(vector<vector<double>> A, double num)
{
  int h = A.size();
  int l = A[0].size();
  vector<vector<double>> C;
  C = creatmatrix(h, l);

  for (int i = 0; i < A.size(); i++)
  {
    for (int j = 0; j < A[i].size(); j++)
    {
      C[i][j] = A[i][j] * num;
    }
  }
  return C;
}

//矩阵相减
vector<vector<double>> minusMatrix(vector<vector<double>> A, vector<vector<double>> B)
{
  int h = A.size();
  int l = A[0].size();
  vector<vector<double>> C;
  C = creatmatrix(h, l);

  for (int i = 0; i < h; i++)
  {
    for (int j = 0; j < l; j++)
    {
      C[i][j] = A[i][j] - B[i][j];
      if (abs(C[i][j]) < epsilon)
      {
        C[i][j] = 0.0;
      }
    }
  }
  return C;
}

//转置矩阵AT
vector<vector<double>> trans(vector<vector<double>> A)
{
  vector<vector<double>> AT = creatmatrix(A[0].size(), A.size());
  int h = AT.size();
  int l = AT[0].size();
  for (int i = 0; i < h; i++)
  {
    for (int j = 0; j < l; j++)
    {
      AT[i][j] = A[j][i];
    }
  }
  return AT;
}

//逆矩阵inv_A
vector<vector<double>> inverse(vector<vector<double>> A)
{
  if (A.size() != A[0].size())
  {
    cout << "输入矩阵维数不合法" << endl;
    exit(0);
  }
  int n = A.size();
  vector<vector<double>> inv_A = creatmatrix(n, n);
  vector<vector<double>> L = creatmatrix(n, n);
  vector<vector<double>> U = creatmatrix(n, n);
  vector<vector<double>> inv_L = creatmatrix(n, n);
  vector<vector<double>> inv_U = creatmatrix(n, n);
  // L矩阵对角元素为1
  for (int i = 0; i < n; i++)
  {
    L[i][i] = 1;
  }
  // U矩阵第一行
  for (int i = 0; i < n; i++)
  {
    U[0][i] = A[0][i];
  }
  // L矩阵第一列
  for (int i = 1; i < n; i++)
  {
    L[i][0] = 1.0 * A[i][0] / A[0][0];
  }

  //计算LU上下三角
  for (int i = 1; i < n; i++)
  {
    //计算U（i行j列）
    for (int j = i; j < n; j++)
    {
      double tem = 0;
      for (int k = 0; k < i; k++)
      {
        tem += L[i][k] * U[k][j];
      }
      U[i][j] = A[i][j] - tem;
      if (abs(U[i][j]) < epsilon)
      {
        U[i][j] = 0.0;
      }
    }
    //计算L（j行i列）
    for (int j = i; j < n; j++)
    {
      double tem = 0;
      for (int k = 0; k < i; k++)
      {
        tem += L[j][k] * U[k][i];
      }
      L[j][i] = 1.0 * (A[j][i] - tem) / U[i][i];
      if (abs(L[i][j]) < epsilon)
      {
        L[i][j] = 0.0;
      }
    }
  }
  // L U剩余位置设为0
  for (int i = 0; i < n; i++)
  {
    for (int j = 0; j < n; j++)
    {
      if (i > j)
      {
        U[i][j] = 0.0;
      }
      else if (i < j)
      {
        L[i][j] = 0.0;
      }
    }
  }
  // LU求逆
  //求矩阵U的逆
  for (int i = 0; i < n; i++)
  {
    inv_U[i][i] = 1 / U[i][i]; // U对角元素的值，直接取倒数
    for (int k = i - 1; k >= 0; k--)
    {
      double s = 0;
      for (int j = k + 1; j <= i; j++)
      {
        s = s + U[k][j] * inv_U[j][i];
      }
      inv_U[k][i] = -s / U[k][k]; //迭代计算，按列倒序依次得到每一个值，
      if (abs(inv_U[k][i]) < epsilon)
      {
        inv_U[k][i] = 0.0;
      }
    }
  }
  //求矩阵L的逆
  for (int i = 0; i < n; i++)
  {
    inv_L[i][i] = 1; // L对角元素的值，直接取倒数，这里为1
    for (int k = i + 1; k < n; k++)
    {
      for (int j = i; j <= k - 1; j++)
      {
        inv_L[k][i] = inv_L[k][i] - L[k][j] * inv_L[j][i];
        if (abs(inv_L[k][i]) < epsilon)
        {
          inv_L[k][i] = 0.0;
        }
      }
    }
  }
  inv_A = multiply(inv_U, inv_L);
  return inv_A;
}

//显示矩阵
void show_matrix(vector<vector<double>> A)
{
  int h = A.size();
  int l = A[0].size();
  cout << "---------" << endl;
  for (int i = 0; i < h; i++)
  {
    for (int j = 0; j < l; j++)
    {
      cout << A[i][j] << "\t";
    }
    cout << endl;
  }
  cout << "---------" << endl;
}

//单个数值的矩阵取出数值
double getnum(vector<vector<double>> A)
{
  return A[0][0];
}

int uart_open(int fd_imu, const char *pathname)
{
  fd_imu = open(pathname, O_RDWR | O_NOCTTY);
  if (-1 == fd_imu)
  {
    perror("Can't Open Serial Port");
    return (-1);
  }
  else
  {
    printf("open %s success!\n", pathname);
  }
  /*测试是否为终端设备*/
  if (isatty(STDIN_FILENO) == 0)
    printf("standard input is not a terminal device\n");
  else
    printf("isatty success!\n");
  return fd_imu;
}

//串口名、波特率、数据位、奇偶校验位、停止位
int uart_set(int fd, int nSpeed, int nBits, char nEvent, int nStop)
{
  struct termios newtio, oldtio;
  /*保存测试现有串口参数设置，在这里如果串口号等出错，会有相关的出错信息*/
  if (tcgetattr(fd, &oldtio) != 0)
  {
    perror("SetupSerial 1");
    printf("tcgetattr(fd,&oldtio) -> %d\n", tcgetattr(fd, &oldtio));
    return -1;
  }
  bzero(&newtio, sizeof(newtio));
  /*步骤一，设置字符大小*/
  newtio.c_cflag |= CLOCAL | CREAD;
  newtio.c_cflag &= ~CSIZE;
  /*设置数据位*/
  switch (nBits)
  {
  case 7:
    newtio.c_cflag |= CS7;
    break;
  case 8:
    newtio.c_cflag |= CS8;
    break;
  }
  /*设置奇偶校验位*/
  switch (nEvent)
  {
  case 'o':
  case 'O': //奇数
    newtio.c_cflag |= PARENB;
    newtio.c_cflag |= PARODD;
    newtio.c_iflag |= (INPCK | ISTRIP);
    break;
  case 'e':
  case 'E': //偶数
    newtio.c_iflag |= (INPCK | ISTRIP);
    newtio.c_cflag |= PARENB;
    newtio.c_cflag &= ~PARODD;
    break;
  case 'n':
  case 'N': //无奇偶校验位
    newtio.c_cflag &= ~PARENB;
    break;
  default:
    break;
  }
  /*设置波特率*/
  switch (nSpeed)
  {
  case 2400:
    cfsetispeed(&newtio, B2400);
    cfsetospeed(&newtio, B2400);
    break;
  case 4800:
    cfsetispeed(&newtio, B4800);
    cfsetospeed(&newtio, B4800);
    break;
  case 9600:
    cfsetispeed(&newtio, B9600);
    cfsetospeed(&newtio, B9600);
    break;
  case 115200:
    cfsetispeed(&newtio, B115200);
    cfsetospeed(&newtio, B115200);
    break;
  case 460800:
    cfsetispeed(&newtio, B460800);
    cfsetospeed(&newtio, B460800);
    break;
  case 921600:
    cfsetispeed(&newtio, B921600);
    cfsetospeed(&newtio, B921600);
    break;
  default:
    cfsetispeed(&newtio, B9600);
    cfsetospeed(&newtio, B9600);
    break;
  }
  /*设置停止位*/
  if (nStop == 1)
    newtio.c_cflag &= ~CSTOPB;
  else if (nStop == 2)
    newtio.c_cflag |= CSTOPB;
  /*设置等待时间和最小接收字符*/
  newtio.c_cc[VTIME] = 0;
  newtio.c_cc[VMIN] = 0;
  /*处理未接收字符*/
  tcflush(fd, TCIFLUSH);
  /*激活新配置*/
  if ((tcsetattr(fd, TCSANOW, &newtio)) != 0)
  {
    perror("com set error");
    return -1;
  }
  printf("set done!\n");
  return 0;
}

int uart_close(int fd)
{
  assert(fd);
  close(fd);
  /*可以在这里做些清理工作*/
  return 0;
}

int writeData(int fd, char *send_buffer, int length)
{
  length = write(fd, send_buffer, length * sizeof(unsigned char));
  return length;
}

int readData(int fd, char *recv_buffer, int length)
{
  length = read(fd, recv_buffer, length);
  return length;
}

void ParseData(char chr) //处理数据
{
  static char chrBuf[29];
  static unsigned char chrCnt = 0;
  signed short sData[12];
  int i;

  chrBuf[chrCnt++] = chr;
  if (chrCnt < 29) //满29字节进行下一步
  {
    return;
  }

  if ((chrBuf[0] != 0x50) || (chrBuf[1] != 0x03))
  {
    printf("imudata Error !!\n");
    memcpy(&chrBuf[0], &chrBuf[1], 10);
    chrCnt--;
    return;
  }

  for (i = 0; i < 12; i++)
  {
    sData[i] = (short)((chrBuf[3 + i * 2] << 8) | chrBuf[4 + i * 2]);
  }

  for (i = 0; i < 3; i++)
  {
    a[i] = (float)(sData[i]) / 32768.0 * 16.0;          //加速度
    w[i] = (float)(sData[3 + i]) / 32768.0 * 2000.0;    //角速度
    Angle[i] = (float)(sData[9 + i]) / 32768.0 * 180.0; //角度
                                                        // h[i] = (int)(sData[6+i])*1.0;//磁场
  }

  chrCnt = 0;
  return;
}

//得到gps经度
void getX(char *chr)
{
  int st = -1;
  int ed = -1;
  for (int i = 0; i < gps_data_size; i++)
  {
    if ((chr[i] == 'N' || chr[i] == 'S') && st == -1)
    {
      st = i + 2;
    }
    else if ((chr[i] == 'E' || chr[i] == 'W') && ed == -1)
    {
      ed = i - 2;
    }
  }

  if (st == -1 || ed == -1)
  {
    printf("gpsX process error!!\n");
    return;
  }

  string gps1(chr);
  gps1 = gps1.substr(st, ed - st + 1);
  gpsData[0] = gps1;
}

//得到gps纬度
void getY(char *chr)
{
  int st = -1;
  int ed = -1;
  int cnt = 0;
  for (int i = 0; i < gps_data_size; i++)
  {
    if (cnt < 2)
    {
      if (chr[i] == ',')
        cnt++;
      continue;
    }
    else if (cnt == 2 && st == -1)
    {
      st = i;
    }

    if (chr[i] == 'N' && ed == -1)
    {
      ed = i - 2;
    }
  }

  if (st == -1 || ed == -1)
  {
    printf("gpsY process error!! %d %d \n", st, ed);
    return;
  }

  string gps2(chr);
  gps2 = gps2.substr(st, ed - st + 1);
  gpsData[1] = gps2;
}

//得到gps高程
void getZ(char *chr)
{
  int st = -1;
  int ed = -1;
  int cnt = 0;
  for (int i = 0; i < gps_data_size; i++)
  {
    if (cnt < 9)
    {
      if (chr[i] == ',')
      {
        cnt++;
      }
      continue;
    }
    else if (cnt == 9 && st == -1)
    {
      st = i;
    }

    if (chr[i] == 'M' && ed == -1)
    {
      ed = i - 2;
    }
  }
  if (st == -1 || ed == -1)
  {
    printf("gpsZ process error!!, st=%d , ed=%d\n", st, ed);
    return;
  }

  string gps3(chr);
  gps3 = gps3.substr(st, ed - st + 1);
  gpsData[2] = gps3;
}

/**
 * t  采样频率
 * K  卡尔曼增益
 * N  先验估计
 * NN 后验估计
 * Q  系统过程噪声协方差矩阵
 * R  测量噪声
 * ka imu中获取的加速度
 * z  gnss中获取的高程
 */

double ka = 0;
double z = 0;
double t = 1;
double R = 0.003112 * t;
vector<vector<double>> N = creatmatrix(2, 1);
vector<vector<double>> F = creatmatrix(2, 2);
vector<vector<double>> B = creatmatrix(2, 1);
vector<vector<double>> P = creatmatrix(2, 2);
vector<vector<double>> Q = creatmatrix(2, 2);
vector<vector<double>> H = creatmatrix(1, 2);
vector<vector<double>> G = creatmatrix(2, 2);
vector<vector<double>> K = creatmatrix(2, 1);
vector<vector<double>> NN = creatmatrix(2, 1);

void kalmanInit()
{
  F[0][0] = 1;
  F[0][1] = t;
  F[1][0] = 0;
  F[1][1] = 1;

  B[0][0] = 1 / 2 * t * t;
  B[1][0] = t;

  P[0][0] = 1;
  P[0][1] = 0;
  P[1][0] = 0;
  P[1][1] = 1;

  Q[0][0] = 0.001;
  Q[0][1] = 0;
  Q[1][0] = 0;
  Q[1][1] = 0.01;

  H[0][0] = 1;
  H[0][1] = 0;

  G[0][0] = 1;
  G[1][1] = 1;
}

void kalmanFilter(double _ka, string _z)
{
  ka = _ka;
  z = stringToNum<double>(_z);
  // Predict
  N = plusMartrix(multiply(F, N), multiplyWithNumber(B, ka));
  P = plusMartrix(multiply(multiply(F, P), trans(F)), Q);
  // Updata
  K = multiplyWithNumber(multiply(P, trans(H)), 1.0 / (getnum(multiply(multiply(H, P), trans(H))) + R));
  NN = plusMartrix(N, multiplyWithNumber(K, z - getnum(multiply(H, N))));
  P = multiply(minusMatrix(G, multiply(K, H)), P);
  N = NN;
}

void usbInit()
{
  // USB口号/dev/ttyUSBn
  fd_imu = uart_open(fd_imu, "/dev/ttyUSBimu");
  if (fd_imu == -1)
  {
    fprintf(stderr, "imu_open error\n");
    exit(EXIT_FAILURE);
  }

  fd_gps = uart_open(fd_gps, "/dev/ttyUSBgps");
  if (fd_gps == -1)
  {
    fprintf(stderr, "gps_open error\n");
    exit(EXIT_FAILURE);
  }

  //初始化串口（串口名、波特率、数据位、奇偶校验位、停止位）
  if (uart_set(fd_imu, BORD_IMU, 8, 'N', 1) == -1)
  {
    fprintf(stderr, "imu set failed!\n");
    exit(EXIT_FAILURE);
  }

  if (uart_set(fd_gps, BORD_GPS, 8, 'N', 1) == -1)
  {
    fprintf(stderr, "gps set failed!\n");
    exit(EXIT_FAILURE);
  }
}

string getTime()
{
  time_t now1 = time(NULL);
  tm *tm_t = localtime(&now1);
  stringstream ss;
  ss << tm_t->tm_year + 1900 << ((tm_t->tm_mon + 1) > 9 ? "-" : "-0") << tm_t->tm_mon + 1 << ((tm_t->tm_mday) > 9 ? "-" : "-0") << tm_t->tm_mday
     << ((tm_t->tm_hour) > 9 ? " " : " 0") << tm_t->tm_hour << ((tm_t->tm_min) > 9 ? ":" : ":0") << tm_t->tm_min << ((tm_t->tm_sec) > 9 ? ":" : ":0") << tm_t->tm_sec;
  return ss.str();
}

float pos_z;

void printGpsDataAll(char *chr)
{
  for (int i = 0; i < gps_data_size; i++)
  {
    printf("%c", chr[i]);
  }
}

int main(void)
{

  char r_buf[1024];
  char gps_r_buf[gps_data_size];
  bzero(r_buf, 1024);
  bzero(gps_r_buf, gps_data_size);
  // usb初始化
  usbInit();

  int RW_flag = 0;
  int cnt = 0;

  ofstream OutFile(getTime());
  while (1)
  {
    if (RW_flag == 0)
    {
      ret = writeData(fd_imu, accbyteSend, 8);
      if (ret == -1)
      {
        printf("imu write failed!\n");
        exit(EXIT_FAILURE);
      }
      // hz=20
      usleep(50000);
      RW_flag = 1;
      cnt++;
    }
    else if (RW_flag == 1)
    {
      bzero(r_buf, 1024);
      ret = readData(fd_imu, r_buf, 29);
      if (ret == -1)
      {
        printf("imu read failed!\n");
        exit(EXIT_FAILURE);
      }

      for (int i = 0; i < ret; i++)
      {
        ParseData(r_buf[i]);
      }

      bzero(gps_r_buf, gps_data_size);
      ret = readData(fd_gps, gps_r_buf, gps_data_size);
      if (ret == -1)
      {
        printf("uart read failed!\n");
        exit(EXIT_FAILURE);
      }
      //放弃前20次数据，用来稳定读数
      if (cnt >= 20 && ret != 0)
      {
        // printGpsDataAll(gps_r_buf);
        getX(gps_r_buf);
        getY(gps_r_buf);
        getZ(gps_r_buf);
        cout << "gps1=" << gpsData[0]
             << " gps2=" << gpsData[1]
             << " gps3=" << gpsData[2]
             << " a0=" << a[0]
             << " a1=" << a[1]
             << " a2=" << a[2]
             << " w0=" << w[0]
             << " w1=" << w[1]
             << " w2=" << w[2]
             << " angle0=" << Angle[0]
             << " angle1=" << Angle[1]
             << " angle2=" << Angle[2]
             << endl;
        OutFile << gpsData[0]
                << " " << gpsData[1]
                << " " << gpsData[2]
                << " " << a[0]
                << " " << a[1]
                << " " << a[2]
                << " " << w[0]
                << " " << w[1]
                << " " << w[2]
                << " " << Angle[0]
                << " " << Angle[1]
                << " " << Angle[2]
                << endl;
      }
      RW_flag = 0;
    }
  }

  OutFile.close();
  ret = uart_close(fd_imu);
  if (ret == -1)
  {
    printf("uart close error\n");
  }
  exit(EXIT_SUCCESS);
}
