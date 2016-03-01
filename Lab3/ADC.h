


//void ADC_Open(unsigned int channelNum);

unsigned short ADC_In(void);

//int ADC_Collect(unsigned int channelNum, unsigned int fs,unsigned short buffer[], unsigned int numberOfSamples);

int ADC_Collect(unsigned int channelNum, unsigned int period, void(*task)(unsigned long data));

void ADC_Init(unsigned int channelNum);
