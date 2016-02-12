


//Opens the channel number to sample for the ADC
void ADC_Open(unsigned int channelNum);

//Takes one sample from the channel selected by ADC_Open
unsigned short ADC_In(void);


//Take n = numberOfSamples samples, using the channel specified and the frequency specified. 
//Will store in the buffer passed as a parameter.
int ADC_Collect(unsigned int channelNum, unsigned int fs,unsigned short buffer[], unsigned int numberOfSamples);

//Initialize the ADC and timer
void ADC0_InitTimer0ATriggerSeq3(uint8_t channelNum, uint32_t period);
