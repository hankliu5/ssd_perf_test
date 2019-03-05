#ifdef __cplusplus
extern "C" {
#endif
enum nvme_opcode {
    nvme_cmd_flush      = 0x00,
    nvme_cmd_write      = 0x01,
    nvme_cmd_read       = 0x02,
    nvme_cmd_readx      = 0x42,
    nvme_cmd_write_uncor    = 0x04,
    nvme_cmd_compare    = 0x05,
    nvme_cmd_dsm        = 0x09,
};

size_t pythonssd_nvme_read(int fd, void *memPtr, size_t size, long file_offset);

#ifdef __cplusplus
}
#endif
