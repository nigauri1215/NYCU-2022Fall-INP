#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include<string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <cstdint>
#include <endian.h>
#include <arpa/inet.h>

using namespace std;

typedef struct{
  uint32_t mnumber;             //uint32_t = usigned int= 32 bit = 4 byte
  uint32_t offset_string;
  uint32_t offset_content;
  uint32_t number_of_files;
} __attribute__((packed))pako_header;
typedef struct{
  uint32_t offset_string;
  uint32_t file_size;
  uint32_t offset_content;
  uint64_t checksum;
} __attribute__((packed))FILE_E;

int main (int argc, char **argv){
    FILE *fp = fopen("testcase.pak","rb");  
    pako_header header;
    FILE_E file;

    fread(&header, sizeof(header), 1, fp);
    cout<<"number of files: "<<header.number_of_files<<endl;
    cout<<endl;

    for(int i=1;i<=header.number_of_files;i++){

      char buffer[100]={0};
      fread(&file,20,1,fp);
      //filename string 
      int filesize=ntohl(file.file_size);
      cout<<"file size: "<<ntohl(file.file_size)<<endl;
      fseek(fp,header.offset_string+file.offset_string,SEEK_SET);
      cout<<"file name: ";
      fread(buffer,100,1,fp);
      for(int j=0;buffer[j]!='\0';j++){
        cout<<buffer[j];
      }
      cout<<endl;
      
      //read the content
      //If the length of the last segment is less than 8 bytes, pad zeros at the end of the segment to ensure its length is correct.
      fseek(fp,header.offset_content+file.offset_content,SEEK_SET);
      char content_buffer[filesize+1]={0};
      fread(content_buffer,filesize,1,fp);
      //Consider each segment as an uint64_t integer. 
      vector<uint64_t>segment;
      char tmp[8]={0};
      uint64_t *value;
      int counter=0;    //count the segment's length
      //cout<<"content: "<<hex<<content_buffer<<endl;
      for(int j=0;j<filesize;j++){
        tmp[counter]=content_buffer[j];
        counter++;
        if(counter==8){
          value=(uint64_t*)tmp;
          //cout <<"value: "<< *value<<endl;
          segment.push_back(*value);
          memset(tmp,0,8);
          memset(value,0,sizeof(uint64_t));
          counter=0;
        }
      }
      
      value=(uint64_t*)tmp;
      //cout <<"value: "<< *value<<endl;
      segment.push_back(*value);


      //checksum
      uint64_t checksum=file.checksum;
      uint64_t segment_xor=0;//segment[0];
      //cout<<"size: "<<segment.size()<<endl;
      for(int k=0;k<segment.size();k++){
        segment_xor=(segment_xor ^ segment[k]);
      }
      //cout<<"xor result: "<<hex<<segment_xor<<endl;
      uint64_t big_sum=0;
      big_sum=be64toh(checksum);

      //cout<<"big: "<<hex<<big_sum<<endl;
      if(big_sum==segment_xor)
      {
        char path[500]={'\0'};
        int k=0;
        string temp=argv[2];
        for(k=0;k<temp.length();k++){
          path[k]=temp[k];
        }
        int index=0;
        while(buffer[index]!='\0'){
          path[k]=buffer[index];
          k++;
          index++;
        }
        
        FILE *p=fopen(path,"w");
        fwrite(content_buffer,1,filesize,p);
        fclose(p);
      }
      //for checker only
      segment.clear();
      cout<<endl;
      fseek(fp,16+i*20,SEEK_SET);
      cout<<endl;
    }

      fclose(fp);
      return 0;
}


/*
Hint:
run checker時要清空dst目錄，然後切到該目錄裡run checker

#include <iostream>
#include <sstream>
#include <cstdint>

int main() {
    uint64_t value;
    std::istringstream iss("18446744073709551610");
    iss >> value;
    std::cout << value;
}

*/
