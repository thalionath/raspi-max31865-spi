
#include <bcm2835.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>

class bcm2835_t
{
public:

	bcm2835_t()
	{
		if( ! ::bcm2835_init() )
		{
			throw std::runtime_error("bcm2835_init failed. Are you running as root?");
		}		
	}

	~bcm2835_t()
	{
		::bcm2835_close();
	}
};

class bcm2835_spi_t
{
public:

	bcm2835_spi_t()
	{
		if( ! ::bcm2835_spi_begin() )
		{
			throw std::runtime_error("bcm2835_spi_begin failedg. Are you running as root?\n");
		}

		::bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
		::bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
		::bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536);
		::bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
		::bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
		::bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, LOW);
	}

	~bcm2835_spi_t()
	{
		::bcm2835_spi_end();
	}

	void chipSelect(int cs)
	{
		::bcm2835_spi_chipSelect(cs);
	}

	std::vector<char> transfer(const std::vector<char>& buffer)
	{
		std::vector<char> response(buffer.size());

		::bcm2835_spi_transfernb(
			const_cast<char*>(buffer.data()),
			response.data(),
			buffer.size()
		);

		return response;
	}
};

class max31865_t
{
	bcm2835_spi_t& spi_;

public:

	explicit max31865_t(bcm2835_spi_t& spi)
	: spi_(spi)
	{
		
	}

	void read_all()
	{
		// read all 8 registers
		for( uint8_t i = 0u; i < 8u; ++i )
		{
			const auto rx = spi_.transfer({ i, 0xFF });

			printf("0x%02X : 0x%02X 0x%02X\n", i, rx[0], rx[1]);
		}
	}

	void enableAutoCoversion()
	{
		spi_.transfer({ 0x80, 0x80 | 0x40 });
	}

	double read()
	{
		// read 3 registers from register address 0x00
		const auto rx = spi_.transfer({ 0x00, 0xFF, 0xFF, 0xFF });
			
		const uint16_t rtd = (rx[2] << 8u) | (rx[3] << 0u);
		const uint16_t adc = rtd >> 1u;

		const double temp = adc * 0.031249727 + (-255.9977596);

		printf("status: 0x%02X, rtd: 0x%04X, adc: %u, temp: %.3f\r\n", rx[1], rtd, adc, temp);

		return temp;
	}
};

int main(int argc, char **argv)
{
	try
	{
		bcm2835_t bcm;
		bcm2835_spi_t spi;
		max31865_t max(spi);

		for( int i = 0; i < 2; ++i )
		{
			spi.chipSelect(i == 0 ? BCM2835_SPI_CS0 : BCM2835_SPI_CS1);

			// read value
			max.read();
		}
	}
	catch( const std::exception& e )
	{
		std::cerr << e.what();
	}

    return 0;
}

