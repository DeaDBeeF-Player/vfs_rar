NAME=vfs_rar
ifeq ($(DEBUG),1)
CPPFLAGS = -g -c -fPIC -Wall -DDEBUG
else
CPPFLAGS = -c -fPIC -Wall
endif
CPPFLAGS += -I./ -I./unrar
LDFLAGS = -shared -fPIC
DST_OBJS = $(NAME).o
RARLIB_OBJS =  $(addprefix unrar/, \
	strlist.o \
	strfn.o \
	pathfn.o \
	savepos.o \
	smallfn.o \
	global.o \
	file.o \
	filefn.o \
	filcreat.o \
	archive.o \
	arcread.o \
	unicode.o \
	system.o \
	isnt.o \
	crypt.o \
	crc.o \
	rawread.o \
	encname.o \
	resource.o \
	match.o \
	timefn.o \
	rdwrfn.o \
	consio.o \
	options.o \
	ulinks.o \
	errhnd.o \
	rarvm.o \
	secpassword.o \
	rijndael.o \
	getbits.o \
	sha1.o \
	extinfo.o \
	extract.o \
	volume.o \
	list.o \
	find.o \
	unpack.o \
	cmddata.o \
	recvol.o scantree.o rs.o filestr.o \
)

$(NAME).so: $(RARLIB_OBJS) $(DST_OBJS)
	g++ $(LDFLAGS) $^ -o $@

install:
	-mkdir -p ~/.local/lib/deadbeef
	-cp $(NAME).so ~/.local/lib/deadbeef

uninstall:
	-rm -rf ~/.local/lib/deadbeef/$(NAME).so

clean:
	-rm -rf *.so *.o unrar/*.o

