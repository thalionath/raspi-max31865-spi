
#include <bcm2835.h>
#include "../vendor/cppzmq/zmq.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

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

	std::vector<uint8_t> read()
	{
		// read 3 registers from register address 0x00
		const auto rx = spi_.transfer({ 0x00, 0xFF, 0xFF, 0xFF });
			
		const uint16_t rtd = (rx[2] << 8u) | (rx[3] << 0u);
		const uint16_t adc = rtd >> 1u;

		// const double temp = adc * 0.031249727 + (-255.9977596);

		// printf("status: 0x%02X, rtd: 0x%04X, adc: %u, temp: %.3f\r\n", rx[1], rtd, adc, temp);

		return std::vector<uint8_t>(rx.begin() + 1, rx.end());
	}
};

static bool run_ = true;

static void signal_handler(int sig)
{
	switch( sig )
	{
	case SIGHUP:
		syslog(LOG_WARNING, "Received SIGHUP signal.");
		break;
	case SIGINT:
	case SIGTERM:
		syslog(LOG_INFO, "Daemon exiting");
		run_ = false;		
		break;
	default:
		syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
		break;
	}
}

static int daemonize(const char* workdir, const char* pid_filename)
{
	// check if parent process id is set
	/*
	if( ::getppid() == 1 )
	{
		return; //  we are already a daemon
	}
	*/

	sigset_t sig_set;

	// set signal mask
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGCHLD);				// ignore child - i.e. we don't need to wait for it
	sigaddset(&sig_set, SIGTSTP);				// ignore Tty stop signals
	sigaddset(&sig_set, SIGTTOU);				// ignore Tty background writes
	sigaddset(&sig_set, SIGTTIN);				// ignore Tty background reads

	sigprocmask(SIG_BLOCK, &sig_set, NULL);	// block the above specified signals

	struct sigaction sig_handler;

	// set up a signal handler
	sig_handler.sa_handler = signal_handler;
	sigemptyset(&sig_handler.sa_mask);
	sig_handler.sa_flags = 0;	

	// signals to handle
	sigaction(SIGHUP, &sig_handler, NULL);     // catch hangup signal
	sigaction(SIGTERM, &sig_handler, NULL);    // catch term signal
	sigaction(SIGINT, &sig_handler, NULL);     // catch interrupt signal


	// fork
	auto pid = fork();

	if( pid < 0 )
	{
		exit(EXIT_FAILURE); // fork failed
	}

	if( pid > 0 )
	{
		exit(EXIT_SUCCESS); // parent exit
	}

	// set file permissions 750
	umask(027); 

	// get a new process group
	auto sid = setsid();

	if( sid < 0 )
	{
		exit(EXIT_FAILURE);
	}

	// close all descriptors
	for( auto i = ::getdtablesize(); i >= 0; --i )
	{
		::close(i);
	}

	// route I/O connections

	// open stdin
	auto i = open("/dev/null", O_RDWR);

	dup(i); // stdout
	dup(i); // stderr

	chdir(workdir); // change working directory

	// create lockfile
	auto pid_file = ::open(pid_filename, O_RDWR | O_CREAT, 0600);

	if( pid_file == -1 )
	{
		syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pid_filename);
		exit(EXIT_FAILURE);
	}

	// lock file
	if( lockf(pid_file, F_TLOCK, 0) == -1 )
	{
		/* Couldn't get lock on lock file */
		syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pid_filename);
		exit(EXIT_FAILURE);
	}

	char buffer[32];

	const auto size = sprintf(buffer, "%d\n", getpid());

	if( size )
	{
		::write(pid_file, buffer, size);
	}

	return pid_file;
}


int main(int argc, char **argv)
{
	auto exit_code = EXIT_SUCCESS;

	auto lockfile = daemonize("/tmp", "max31865d.pid");

	try
	{
		zmq::context_t ctx(1);
		zmq::socket_t publisher(ctx, ZMQ_PUB);

		publisher.bind("tcp://*:3000");

		bcm2835_t bcm;
		bcm2835_spi_t spi;
		max31865_t max(spi);

		spi.chipSelect(BCM2835_SPI_CS0);
		max.enableAutoCoversion();

		spi.chipSelect(BCM2835_SPI_CS1);
		max.enableAutoCoversion();

		const std::string names[] = {
			"0",
			"1"
		};

		while( run_ )
		{
			for( int i = 0; i < 2; ++i )
			{
				spi.chipSelect(i == 0 ? BCM2835_SPI_CS0 : BCM2835_SPI_CS1);

				const auto value = max.read();

				// send envelope
				publisher.send(
					names[i].c_str(),
					names[i].length(),
					ZMQ_SNDMORE
				);

				publisher.send(
					value.begin(),
					value.end()
				);				
			}

			sleep(1);
		}		
	}
	catch( const std::exception& e )
	{
		// std::cerr << e.what();

		syslog(LOG_ERR, e.what());

		exit_code = EXIT_FAILURE;
	}

	::close(lockfile);

	exit(exit_code);

    return 0;
}

