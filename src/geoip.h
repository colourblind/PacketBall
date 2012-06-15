#ifndef COLOURBLIND_GEOIP_H
#define COLOURBLIND_GEOIP_H

#include <string>
#include <fstream>

struct Location
{
	std::string countryCode;
	std::string countryName;
	std::string region;
	std::string city;
	std::string postalCode;
    float latitude;
    float longitude;
};

class GeoIp
{
public:
	GeoIp();
	GeoIp(std::string path);
	~GeoIp();

	bool Init();
	Location Lookup(std::string ip);
	Location Lookup(unsigned char *ip);
	Location Lookup(unsigned int ip);

private:
	int SeekCountry(unsigned int ip);

	std::ifstream dataFile_;
	unsigned int segmentCount_;
	unsigned int recordLength_;
};

const int COUNTRY_EDITION = 1;
const int REGION_EDITION_REV0 = 7;
const int REGION_EDITION_REV1 = 3;
const int CITY_EDITION_REV0 = 6;
const int CITY_EDITION_REV1 = 2;
const int ORG_EDITION = 5;
const int ISP_EDITION = 4;
const int PROXY_EDITION = 8;
const int ASNUM_EDITION = 9;
const int NETSPEED_EDITION = 10;
const int DOMAIN_EDITION = 11;
const int COUNTRY_EDITION_V6 = 12;
const int ASNUM_EDITION_V6 = 21;
const int ISP_EDITION_V6 = 22;
const int ORG_EDITION_V6 = 23;
const int DOMAIN_EDITION_V6 = 24;
const int CITY_EDITION_REV1_V6 = 30;
const int CITY_EDITION_REV0_V6 = 31;
const int NETSPEED_EDITION_REV1 = 32;
const int NETSPEED_EDITION_REV1_V6 = 33;

const int COUNTRY_BEGIN = 16776960;
const int STATE_BEGIN   = 16700000;
const int STRUCTURE_INFO_MAX_SIZE = 20;
const int DATABASE_INFO_MAX_SIZE = 100;
const int FULL_RECORD_LENGTH = 100;//???
const int SEGMENT_RECORD_LENGTH = 3;
const int STANDARD_RECORD_LENGTH = 3;
const int ORG_RECORD_LENGTH = 4;
const int MAX_RECORD_LENGTH = 4;
const int MAX_ORG_RECORD_LENGTH = 1000;//???
const int FIPS_RANGE = 360;
const int STATE_BEGIN_REV0 = 16700000;
const int STATE_BEGIN_REV1 = 16000000;
const int US_OFFSET = 1;
const int CANADA_OFFSET = 677;
const int WORLD_OFFSET = 1353;
const int GEOIP_STANDARD = 0;
const int GEOIP_MEMORY_CACHE = 1;
const int GEOIP_UNKNOWN_SPEED = 0;
const int GEOIP_DIALUP_SPEED = 1;
const int GEOIP_CABLEDSL_SPEED = 2;
const int GEOIP_CORPORATE_SPEED = 3;

#endif // COLOURBLIND_GEOIP_H
