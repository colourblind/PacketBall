#include <iostream>
#include <sstream>
#include "geoip.h"
#include "lookup.h"

using namespace std;

GeoIp::GeoIp() : 
	dataFile_("GeoLiteCity.dat", ios::in|ios::binary), 
	segmentCount_(0) 
{

}

GeoIp::GeoIp(string path) :
	dataFile_(path.c_str(), ios::in|ios::binary), 
	segmentCount_(0) 
{

}

GeoIp::~GeoIp()
{
	dataFile_.close();
}

bool GeoIp::Init()
{
	unsigned char buffer[SEGMENT_RECORD_LENGTH];
	
	recordLength_ = STANDARD_RECORD_LENGTH;

    dataFile_.seekg(-SEGMENT_RECORD_LENGTH, ios::end);
	dataFile_.read((char *)&buffer[0], SEGMENT_RECORD_LENGTH);
	for (int i = 0; i < SEGMENT_RECORD_LENGTH; i ++)
		segmentCount_ += (int)buffer[i] << (8 * i);

	return true;
}

Location GeoIp::Lookup(string ip)
{
    stringstream ipString(ip);
    unsigned char ipBytes[4];
    for (int i = 0; i < 4; i ++)
    {
        string octet;
        getline(ipString, octet, '.');
        ipBytes[i] = (unsigned char)atoi(octet.c_str());
    }
	return Lookup(ipBytes);
}

Location GeoIp::Lookup(unsigned char *ipBytes)
{
	unsigned int ip = (ipBytes[0] << 24) + (ipBytes[1] << 16) + (ipBytes[2] << 8) + ipBytes[3];
	return Lookup(ip);
}

Location GeoIp::Lookup(unsigned int ip)
{
	Location result;

	unsigned char buffer[FULL_RECORD_LENGTH];
	char *buffPtr = (char *)buffer;
	int stringLen = 0;

	int countryPtr = SeekCountry(ip);
	if (countryPtr < 0)
		return result;

	int recordPtr = countryPtr + (2 * recordLength_ - 1) * segmentCount_;

	dataFile_.seekg(recordPtr);
	dataFile_.read((char *)&buffer[0], FULL_RECORD_LENGTH);

	int countryIndex = (int)buffer[0];
	result.countryCode = COUNTRY_CODE[countryIndex];
	result.countryName = COUNTRY_NAME[countryIndex];
	buffPtr += 1;

	stringLen = strchr(buffPtr, '\0') - buffPtr;
	result.region = string(buffPtr, stringLen);
	buffPtr += stringLen + 1;

	stringLen = strchr(buffPtr, '\0') - buffPtr;
	result.city = string(buffPtr, stringLen);
	buffPtr += stringLen + 1;

    stringLen = strchr(buffPtr, '\0') - buffPtr;
    result.postalCode = string(buffPtr, stringLen);
    buffPtr += stringLen + 1;

    unsigned int lat = 0;
    for (int i = 0; i < 3; i ++, buffPtr ++)
        lat += *buffPtr << (i * 8);
    result.latitude = (float)lat / 10000 - 180;

    unsigned int lon = 0;
    for (int i = 0; i < 3; i ++, buffPtr ++)
        lon += *buffPtr << (i * 8);
    result.longitude = (float)lon / 10000 - 180;

	return result;
}

int GeoIp::SeekCountry(unsigned int ip)
{
	unsigned char buffer[2 * MAX_RECORD_LENGTH];
	int offset = 0;
    int x[2];

	for (int depth = 31; depth >= 0; depth --)
	{
		dataFile_.seekg(2 * recordLength_ * offset);
		dataFile_.read((char *)&buffer[0], 2 * MAX_RECORD_LENGTH);

        for (int i = 0; i < 2; i ++)
        {
            x[i] = 0;
            for (int j = 0; j < recordLength_; j ++)
            {
                int y = buffer[i * recordLength_ + j];
                if (y < 0)
                    y += 256;
                x[i] += y << (j * 8);
            }
        }

        if ((ip & (1 << depth)) > 0)
        {
            if (x[1] >= segmentCount_)
                return x[1];
            offset = x[1];
        }
        else
        {
            if (x[0] >= segmentCount_)
                return x[0];
            offset = x[0];
        }
	}

	return -1;
}
