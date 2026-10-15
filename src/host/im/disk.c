/* Preserve the complete DC42 image, including Lisa sector tags. The initial
 * frontend loads a working copy into RAM; changed sectors go straight to the
 * worker disk API so its existing persistence and download paths can be used. */
#include <vars.h>
#include <emscripten.h>

extern uint8 *dc42_read_sector_data(DC42ImageType *, uint32);
extern uint8 *dc42_read_sector_tags(DC42ImageType *, uint32);

typedef struct Disk {
    DC42ImageType *image;
    int dirty;
    struct Disk *next;
} Disk;
static Disk *disks;

EM_JS(int, disk_open, (const char *name), {
    return workerApi.disks.open(UTF8ToString(name));
});
EM_JS(int, disk_size, (int id), { return workerApi.disks.size(id); });
EM_JS(int, disk_read, (int id, void *buf, int offset, int size), {
    return workerApi.disks.read(id, buf, offset, size);
});
EM_JS(int, disk_write, (int id, const void *buf, int offset, int size), {
    return workerApi.disks.write(id, buf, offset, size);
});
EM_JS(void, disk_close, (int id), { workerApi.disks.close(id, true); });

static int error(DC42ImageType *f, const char *message)
{
    snprintf(f->returnmsg, sizeof(f->returnmsg), "%s", message);
    f->errormsg = f->returnmsg;
    return f->retval = -1;
}

static int write_sector(DC42ImageType *f, uint32 sector, uint8 *data, int tags)
{
    if (sector >= f->numblocks || f->readonly)
        return error(f, "Invalid sector or read-only disk");
    unsigned size = tags ? f->tagsize : f->sectorsize;
    unsigned offset = (tags ? f->tagstart : f->sectoroffset) + sector * size;
    if (disk_write(f->fd - 3, data, offset, size) != size)
        return error(f, "Short worker disk write");
    memcpy(f->RAM + offset, data, size);
    for (Disk *d = disks; d; d = d->next)
        if (d->image == f) d->dirty = 1;
    return f->retval = 0;
}
static int write_data(DC42ImageType *f, uint32 sector, uint8 *data)
{ return write_sector(f, sector, data, 0); }
static int write_tags(DC42ImageType *f, uint32 sector, uint8 *data)
{ return write_sector(f, sector, data, 1); }

static void flush(Disk *d)
{
    if (!d->dirty) return;
    DC42ImageType *f = d->image;
    dc42_recalc_checksums(f);
    if (disk_write(f->fd - 3, f->RAM + 72, 72, 8) != 8) {
        error(f, "Could not save DC42 checksums");
        return;
    }
    d->dirty = 0;
}

/* Call before entering workerApi: periodic tasks can stop the worker or offer
 * a disk download, and both should see up-to-date DC42 checksums. */
void im_disks_flush(void)
{
    for (Disk *d = disks; d; d = d->next) flush(d);
}

static int close_image(DC42ImageType *f)
{
    Disk **link = &disks;
    while (*link && (*link)->image != f) link = &(*link)->next;
    if (!*link) return 0;
    Disk *d = *link;
    flush(d);
    /* Export only after the guest has put away the floppy and checksums have
     * been refreshed. Keep the tagged container intact for native LisaEm. */
    if (f->tagsize == 12) {
        EM_ASM({ workerApi.didEjectFloppy(UTF8ToString($0), $1, $2); }, f->fname, f->RAM, f->size);
    }
    *link = d->next;
    free(d);
    disk_close(f->fd - 3);
    free(f->RAM);
    f->RAM = NULL;
    f->fd = -1;
    return 0;
}

void im_disks_close(void)
{
    while (disks) close_image(disks->image);
}

static uint32 be32(const uint8 *p)
{ return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | (p[2] << 8) | p[3]; }

int im_dc42_open(DC42ImageType *f, char *name, char *options)
{
    memset(f, 0, sizeof(*f));
    f->fd = -1;
    int id = disk_open(name);
    if (id < 0) return error(f, "Worker disk not found");
    int size = disk_size(id);
    uint8 header[84];
    if (size < 84 || size > 64 * 1024 * 1024 ||
        disk_read(id, header, 0, 84) != 84) goto invalid;
    uint32 data_size = be32(header + 64), tag_size = be32(header + 68);
    if (header[0] > 63 || header[82] != 1 || header[83] != 0 ||
        data_size == 0 || data_size % 512 ||
        (uint64_t)data_size + tag_size + 84 != size) goto invalid;
    uint32 sectors = data_size / 512;
    if (tag_size != sectors * 12 && tag_size != sectors * 20) goto invalid;
    if (tag_size == sectors * 12 && sectors != 800 && sectors != 1600) goto invalid;
    uint8 *image_bytes = malloc(size);
    if (!image_bytes) goto invalid;
    if (disk_read(id, image_bytes, 0, size) != size) { free(image_bytes); goto invalid; }
    Disk *d = calloc(1, sizeof(*d));
    if (!d) { free(image_bytes); goto invalid; }
    f->fd = id + 3; /* Core uses fd > 2 to indicate an inserted disk. */
    f->RAM = image_bytes;
    f->size = size;
    f->mmappedio = 2;
    f->readonly = strchr(options, 'r') != NULL;
    snprintf(f->fname, sizeof(f->fname), "%s", name);
    f->datasize = f->sectorsize = 512;
    f->datasizetotal = data_size;
    f->tagsizetotal = tag_size;
    f->sectoroffset = 84;
    f->tagstart = 84 + data_size;
    f->numblocks = sectors;
    f->tagsize = tag_size / sectors;
    f->ftype = sectors == 1600 ? SONY800KFLOPPY : SONY400KFLOPPY;
    f->maxtrk = 80;
    f->maxsec = 15;
    f->maxside = sectors == 1600 ? 2 : 1;
    f->read_sector_data = dc42_read_sector_data;
    f->read_sector_tags = dc42_read_sector_tags;
    f->write_sector_data = write_data;
    f->write_sector_tags = write_tags;
    f->close_image = f->close_image_by_handle = close_image;
    f->errormsg = f->returnmsg;
    d->image = f;
    d->next = disks;
    disks = d;
    return 0;
invalid:
    disk_close(id);
    return error(f, "Expected a tagged DC42 disk image of at most 64 MB");
}
