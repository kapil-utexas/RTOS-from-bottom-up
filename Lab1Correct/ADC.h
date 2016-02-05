


void ADC_Open(unsigned int channelNum);

unsigned short ADC_In(void);

int ADC_Collect(unsigned int channelNum, unsigned int fs,unsigned short buffer[], unsigned int numberOfSamples);

void ADC0_InitTimer0ATriggerSeq3(uint8_t channelNum, uint32_t period);
