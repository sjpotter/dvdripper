#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>

int main()
{
	dvd_struct s;
	
	int layer = 0;
	int fd;

	if ( (fd = open("/dev/dvd", O_RDONLY)) == -1) {
		perror("failed to open /dev/dvd");
	}

	while (layer < 2) {
		s.type = DVD_STRUCT_PHYSICAL;
		s.physical.layer_num = layer;

		if ( ioctl(fd, DVD_READ_STRUCT, &s) == -1) {
			perror("ioctl failed");
		}
	
		printf("layer = %d\nbook_version = %d, book_type = %d, min_rate = %d, disc_size = %d, layer_type = %d, track_path = %d, nlayers = %d, track_density = %d, linear_density = %d, bca = %d, start_sector = %u, end_sector = %u, end_sector_l0 = %u\n\n", layer, s.physical.layer[layer].book_version, s.physical.layer[layer].book_type, s.physical.layer[layer].min_rate, s.physical.layer[layer].disc_size, s.physical.layer[layer].layer_type, s.physical.layer[layer].track_path, s.physical.layer[layer].nlayers, s.physical.layer[layer].track_density, s.physical.layer[layer].linear_density, s.physical.layer[layer].bca, s.physical.layer[layer].start_sector, s.physical.layer[layer].end_sector, s.physical.layer[layer].end_sector_l0);

		bzero(&s, sizeof(dvd_struct));
		layer++;
	}
}
