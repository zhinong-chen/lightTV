/******************************温湿度**************************/
#include<DHT.h>
#define DHTPIN 26
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

/*************************双核调度********************************/

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

void astronautone( void *pvParameters );
void showinfo( void *pvParameters );

/**********************************************************/
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
/***********************屏幕驱动显示********************/
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
#include "image.h"
#include "astronautone.h"
#include "font.h"
#include <pgmspace.h>

#define TFT_SCL         14
#define TFT_SDA         13
#define TFT_CS          15
#define TFT_RST         27  //27
#define TFT_DC          2
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_SDA,TFT_SCL ,TFT_RST);

/**************************服务器ip地址****************/
#include <EEPROM.h>
#define EEPROM_Size 4096  //字节

#define cmp_addr 0  //电脑服务器ip地址存储位置
#define phone_addr 5//手机服务器ip地址存储位置
const String localip="";

IPAddress ip;
IPAddress serverip;

const int16_t colors[]={
  ST77XX_BLACK,ST77XX_WHITE,ST77XX_RED,ST77XX_BLUE,ST77XX_GREEN,ST77XX_YELLOW,ST77XX_ORANGE
};
int16_t onoffcolor=ST77XX_BLACK;

int init_ip[4];
String buff="";
String cinf[5];     //[cpu, free, total, memory, now_time]
String buffcinf[5]="";    //数据缓存
int datetime[3]={0};      //存储时间 时-分-星期
int buffdatetime[3]={0};  //时间缓存
unsigned long onoff=0;    //时间冒号的亮与灭

unsigned long touch_start=0;
int pictureNum=0;           //控制旋转的太空人播放的帧数
int showmode=0;             // 用来显示模式的状态
bool showlogo=false;        //用来判断logo时候显示
bool change=true;

float h=0.0;
float t=0.0;
float buffht[2]={0};

WiFiClient client_c; //声明一个客户端对象，用于与服务器进行连接
WiFiClient client_p;
WiFiClient client; //声明一个客户端对象，用于与服务器进行连接
WiFiServer server(100);   //创建一个服务器对象

void setup()
{
  initled();
  Serial.begin(115200);
  dht.begin();
  
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(4);
  tft.fillScreen(ST77XX_BLACK);
  Serial.print("屏幕初始化完成！");

  initText(5,10,ST77XX_WHITE);
  tft.print("The screen has been initialized");

  initText(5,30,ST77XX_WHITE);
  tft.print("Initializing EEPROM");
  Serial.println("正在初始化空间");
  while(!EEPROM.begin(EEPROM_Size)){
    Serial.println(".");
  }
  
  initText(100,50,ST77XX_GREEN);
  tft.print("OK");

  initText(5,50,ST77XX_GREEN);
  tft.print("Connecting WiFi");
  Serial.println("准备配网");
  if(!AutoConfig()){ //使用乐鑫官方配网app 手机连接的WiFi频段为2.4G才有效
    SmartConfig();
  }
  ip=WiFi.localIP();
  Serial.print("-----------_");
  Serial.println(ip);
  if(WiFi.status()== WL_CONNECTED){
    initText(5,70,ST77XX_RED);
    tft.print("IP:");
    tft.print(ip);
    initText(5,90,ST77XX_BLUE);
    tft.print("SID:");
    tft.print(WiFi.SSID().c_str());
  }

  printserverip();    //先获取服务器IP地址
  if(!con_server()){
    Serial.println("无法连接服务器,\n 准备开启服务器");
    server_get_ip();
  }
  
   xTaskCreatePinnedToCore(     //使用第二个核心进行 电脑信息，时间数据获取
    astronautone
    ,  "astronautone"   // A name just for humans
    ,  4096  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  0);

  for(int i=2;i<7;i++){
    tft.fillRect(60,110,20,18,colors[0]);
    initText(60,110,colors[i]);
    tft.print(i-1);
    delay(1000);
  }
  tft.fillScreen(ST77XX_BLACK);
  
}

void loop()
{
  touchpin();
  switch(showmode){
    case 0:   //显示时间
      if(change){
        tft.setCursor(20,20);
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_BLUE);
        tft.print(datetime[0]);
      }
      if(buffdatetime[0]!=datetime[0]){         //判断本次数据时候和上次一样，要显示不同的信息才用黑色进行了图像重叠清除，以下同理
        tft.setCursor(20,20);
        tft.fillRect(20,20,25,15,colors[0]);
        tft.setTextColor(ST77XX_BLUE);
        buffdatetime[0]=datetime[0];
        tft.print(datetime[0]);
      }
      
      if(millis()-onoff<0){
        onoff=0;
      }
      if(millis()-onoff>1000){
        onoffcolor=~onoffcolor;
        onoff=millis();
      }
      tft.setCursor(50,20);
      tft.setTextColor(onoffcolor);
      tft.print(":");

      if(change){
        tft.setCursor(65,20);
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_BLUE);
        tft.println(datetime[1]);
      }
      if(buffdatetime[1]!=datetime[1]){
        tft.setCursor(65,20);
        tft.fillRect(65,20,25,15,colors[0]);
        buffdatetime[1]=datetime[1];
        tft.println(datetime[1]);
      }

      if(change){
        tft.drawRGBBitmap(25,40,xing,20,20);
        tft.drawRGBBitmap(45,40,qi,20,20);
        tft.drawRGBBitmap(65,40,dayNum[datetime[2]],20,20);
        change=false;
      }
      if(buffdatetime[2]!=datetime[2]){         //用汉字显示星期
        tft.fillRect(65,40,20,20,colors[0]);
        tft.drawRGBBitmap(65,40,dayNum[datetime[2]],20,20);
        buffdatetime[2]=datetime[2];
      }
     
      break;
    case 1:
      drawinf();
      
      break;
    case 2:   //温湿度
      if(change){
        tft.drawRGBBitmap(5,20,wen,20,20);
        tft.drawRGBBitmap(25,20,du1,20,20);
        tft.drawRGBBitmap(5,40,shi,20,20);
        tft.drawRGBBitmap(25,40,du2,20,20);
        
        tft.setCursor(45,25);
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_RED);
        tft.print(":");
        tft.setCursor(100,30);
        tft.println("C");

        tft.setCursor(45,45);
        tft.setTextColor(ST77XX_GREEN);
        tft.print(":");
        tft.setCursor(100,50);
        tft.println("%");
        
        change=false;
        
      }
      if(t!=buffht[1]){
        tft.setTextSize(1);
        tft.fillRect(60,30,40,10,colors[0]);
        tft.setCursor(60,30 );
        tft.setTextColor(ST77XX_RED);
        tft.print(t);
        buffht[1]=t;
      }
      if(h!=buffht[0]){   
        tft.setTextSize(1);
        tft.fillRect(60,50,40,10,colors[0]);
        tft.setCursor(60,50);
        tft.setTextColor(ST77XX_GREEN);
        tft.print(h);
        buffht[0]=h;
      }
      break;
    case 3:   //显示二维码
      tft.drawRGBBitmap(12,12,zhinong,100,100);
      break;
    case 4:   //显示二维码
      tft.drawRGBBitmap(12,12,xiang,100,100);
      break;
  }

  if(showmode<3){
    tft.drawRGBBitmap(80,80,test[pictureNum],40,42);
    pictureNum++;
    if(pictureNum==21)
      pictureNum=0;
    if(!showlogo)
       logo(5,80);
  }
}

/***************************创建任务********************/

void astronautone(void *pvParameters) {
  (void)pvParameters;
  WiFiClient client; //声明一个客户端对象，用于与服务器进行连接
  
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP,"ntp.ntsc.ac.cn",8*3600,60000);

  timeClient.begin();
  dht.begin();
  client.connect(serverip,1000);
  while(1)
  {
    vTaskDelay(1000);
    /**********************************get computer information********************************/
    Serial.println("尝试访问服务器");
    if (client.connected()) {
      Serial.println("连接成功！");
      client.print("allinfo");
      vTaskDelay(300);
      bool getinf=false;
      while(client.available()){
        String line = client.readStringUntil('\n'); //按行读取数据
        getinf=true;
        Serial.println(line);
        cinf[0]=line.substring(line.indexOf(':')+1,line.indexOf('f'));
    
        line=line.substring(line.indexOf('f'));
        cinf[1]=line.substring(line.indexOf(':')+1,line.indexOf('t'));
        
        line=line.substring(line.indexOf('t'));
        cinf[2]=line.substring(line.indexOf(':')+1,line.indexOf('m'));
    
        line=line.substring(line.indexOf('m'));
        cinf[3]=line.substring(line.indexOf(':')+1,line.indexOf('o'));
    
        line=line.substring(line.indexOf('o'));
        cinf[4]=line.substring(line.indexOf(':')+1);
        
      }
      if(getinf){
        Serial.println("获取到了数据");
      }
    }
    else{
      if(client.connect(serverip,1000)){
        Serial.println("连接成功！");
        client.print("allinfo");
        vTaskDelay(100);
        bool getinf=false;
        while(client.available()){
          String line = client.readStringUntil('\n'); //按行读取数据
          Serial.println(line);
          getinf=true;
          /*******************************字符串解析*********************************/
          cinf[0]=line.substring(line.indexOf(':')+1,line.indexOf('f'));
      
          line=line.substring(line.indexOf('f'));
          cinf[1]=line.substring(line.indexOf(':')+1,line.indexOf('t'));
          
          line=line.substring(line.indexOf('t'));
          cinf[2]=line.substring(line.indexOf(':')+1,line.indexOf('m'));
      
          line=line.substring(line.indexOf('m'));
          cinf[3]=line.substring(line.indexOf(':')+1,line.indexOf('o'));
      
          line=line.substring(line.indexOf('o'));
          cinf[4]=line.substring(line.indexOf(':')+1);
          
        }
        if(getinf){
          Serial.println("获取到了数据");
        }
      }
   }
/***********************************xiu gai ************************/
/*
    if (client.connect(serverip, 1000)) 
    {
        Serial.println("访问成功");
        client.print("allinfo");
        while (client.connected() || client.available()) //如果已连接或有收到的未读取的数据
        {
            if (client.available()) //如果有数据可读取
            {
                String line = client.readStringUntil('\n'); //按行读取数据
                //Serial.println(line);
                /*****************************prase information******************************/
 /*              
                //c:9.9%f:8.16Gt:15.86Gm:48%o:2021-08-02 20:53:12
                cinf[0]=line.substring(line.indexOf(':')+1,line.indexOf('f'));
                //Serial.println("cinf[0]:"+cinf[0]);
                
                //f:8.16Gt:15.86Gm:48%o:2021-08-02 20:53:12
                line=line.substring(line.indexOf('f'));
                cinf[1]=line.substring(line.indexOf(':')+1,line.indexOf('t'));
                //Serial.println("cinf[1]:"+cinf[1]);
                
                //t:15.86Gm:48%o:2021-08-02 20:53:12
                line=line.substring(line.indexOf('t'));
                cinf[2]=line.substring(line.indexOf(':')+1,line.indexOf('m'));
                //Serial.println("cinf[2]:"+cinf[2]);
              
                //m:48%o:2021-08-02 20:53:12
                line=line.substring(line.indexOf('m'));
                cinf[3]=line.substring(line.indexOf(':')+1,line.indexOf('o'));
                //Serial.println("cinf[3]:"+cinf[3]);
              
                //o:2021-08-02 20:53:12
                line=line.substring(line.indexOf('o'));
                cinf[4]=line.substring(line.indexOf(':')+1);
                //Serial.println("cinf[4]:"+cinf[4]);
            }
        }
        client.stop(); //关闭当前连接
    }
    else
    {
        Serial.println("访问失败");
        //client.stop(); //关闭当前连接
    }

  /****************************获取时间**************************************************/
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
  Serial.println(timeClient.getDay());
  datetime[0]=timeClient.getHours();
  datetime[1]=timeClient.getMinutes();
  datetime[2]=timeClient.getDay();

  /***************************************温湿度********************************************/
  h = dht.readHumidity();
  t = dht.readTemperature();
  Serial.print("h:");
  Serial.println(h);
  Serial.print("t:");
  Serial.println(t);
  
  }
}


void server_get_ip(){
//**********************************************
  server.begin();    //开启100号端口进行监听
  Serial.println("开启服务器");

  tft.setCursor(40,100);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("server open");
  
  int count=1;
  bool getaddr=false;
  unsigned int start=millis();
  while(true){
    WiFiClient client=server.available();
    if(client){
      Serial.println("有客户连接");
      while(client.connected()){
        String readbuff="";
        int addr=0;
        if(client.available()){
           String readbuff = client.readStringUntil('\n');
           Serial.println(readbuff.length());
           Serial.println(readbuff.indexOf('c'));
           if(readbuff.charAt(0)=='c'){
              addr=cmp_addr;
           }
           else if(readbuff.charAt(0)=='p'){
              addr=phone_addr;
           }
           String s=readbuff.substring(readbuff.lastIndexOf('.')+1,readbuff.length());
           int c=int(EEPROM.read(addr));
           if(c!=s.toInt()){
            EEPROM.write(addr,s.toInt());
            EEPROM.commit();
            //Serial.println("写入数据"+s);
            client.stop();
            getaddr=true;
            break;
           }
           else{
            //Serial.println("有相同的数据");
            getaddr=true;
            break;
           }
        }
      }
    }
    else{
      client.stop();
    }
    if(getaddr){
      //Serial.println("GET addr ok");
      client.stop();
      break;
    }
    else if(millis()-start>120000){
      //Serial.println("连接超时!");
      tft.setCursor(5,100);
      tft.setTextColor(ST77XX_RED);
      tft.println("waiting long time!");
      client.stop();
      break;
    }
  }
}

void printserverip(){
  Serial.print("服务器IP：");
  for(int i=0;i<3;i++){
    serverip[i]=ip[i];
  }
  serverip[3]=EEPROM.read(cmp_addr);
  Serial.println(serverip);
}

bool con_server(){
  if(client.connect(serverip,1000)){
    Serial.println("能连上服务器");
    client.stop();
    return true;
  }
  Serial.println("无法连接服务器");
  return false;
}

void drawinf(){
  initText(5,15,colors[2]);
  tft.print("cpu:");
  if(cinf[0]!=buffcinf[0]){
    tft.fillRect(27,15,40,10,colors[0]);
    buffcinf[0]=cinf[0];
  }
  tft.println(cinf[0]);
  
  initText(5,25,colors[3]);
  tft.print("free:");
  if(cinf[1]!=buffcinf[1]){
    tft.fillRect(30,25,40,10,colors[0]);
    buffcinf[1]=cinf[1];
  }
  tft.println(cinf[1]);
  
  initText(5,35,colors[4]);
  tft.print("total:");
  if(cinf[2]!=buffcinf[2]){
    tft.fillRect(40,35,50,10,colors[0]);
    buffcinf[2]=cinf[2];
  }
  tft.println(cinf[2]);
  
  initText(5,45,colors[1]);
  tft.print("memory:");
  if(cinf[3]!=buffcinf[3]){
    tft.fillRect(40,45,30,10,colors[0]);
    buffcinf[3]=cinf[3];
  }
  tft.println(cinf[3]);
  
  initText(5,55,colors[1]);
  String date=cinf[4];
  date=date.substring(0,date.indexOf(' '));
  tft.print("date:");
  if(date!=buffcinf[4]){
    tft.fillRect(40,55,80,10,colors[0]);
    buffcinf[4]=date;
  }  
  tft.println(date);
}

void logo(int x,int y){
  initText(x,y,ST77XX_BLUE);
  tft.print("Designed by:");
  initText(x,y+15,ST77XX_RED);
  tft.print("zhi nong.");
  initText(10,y+30,ST77XX_GREEN);
  tft.print("to shit-yan");
  showlogo=true;
}

void initled(){
  pinMode(4,OUTPUT);
  pinMode(32,OUTPUT);
  pinMode(33,OUTPUT);

  digitalWrite(4,LOW);
  digitalWrite(32,LOW);
  digitalWrite(33,LOW);
  
}

void lightled(){
  digitalWrite(4,random(0,2));
  digitalWrite(32,random(0,2));
  digitalWrite(33,random(0,2));
}

void touchpin(){
  if(millis()-touch_start>1500){
    int touch0=touchRead(T0);
    if(touch0<5){
      Serial.println(touch0);  // get value using T0
      showmode++;
      if(showmode==5)
        showmode=0;
      showlogo=false;
      tft.fillScreen(ST77XX_BLACK);
    }
    change=true;
    touch_start=millis();
    lightled();                                                                      
  }
}

void SmartConfig()
{
   WiFi.mode(WIFI_STA);
   Serial.println("\r\n wait for smartconfig....");
   WiFi.beginSmartConfig();
   unsigned long wtime=millis();
   int count=0;
   while(1)
   {
    Serial.print(".");
    delay(500);
    if ( WiFi.smartConfigDone())
    {
      Serial.println("SmartConfig Success");
      Serial.printf("SSID:%s\r\n",WiFi.SSID().c_str());
      Serial.printf("PSW:%s\r\n",WiFi.psk().c_str());
      break;      
    }
    if(millis()-wtime>=1000){
      count++;
      wtime=millis();
    }
    if(count==120)
      break;
   }  
}



bool AutoConfig()
{
  WiFi.begin();
  for (int i=0; i<20; i++)
  {
    int wstatus = WiFi.status();
    if (wstatus == WL_CONNECTED )  
       {
          Serial.println("wifi smartConfig success");
          Serial.printf("SSID:%s",WiFi.SSID().c_str());
          Serial.printf(",PWS:%s\r\n",WiFi.psk().c_str());
          Serial.print("localIP:");
          Serial.println(WiFi.localIP());
          Serial.print(",GateIP:");
          Serial.println(WiFi.gatewayIP()); //网关
          return true;    
       }
       else
       {
          Serial.print("WIFI AutoConfig Waiting ....");
          Serial.println(wstatus);
          delay(1000);  
       }
  }
  Serial.println("Wifi autoconfig faild!");
  return false;
}

void drawpicture(){
  tft.drawRGBBitmap(0,0,shit,128,128);
}

void initText(int x,int y,int16_t color){
  tft.setCursor(x,y);
  tft.setTextSize(1);
  tft.setTextColor(color);
}

void getTempHum(){
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  Serial.print("h:");
  Serial.println(h);
  Serial.print("t:");
  Serial.println(t);
  
}
