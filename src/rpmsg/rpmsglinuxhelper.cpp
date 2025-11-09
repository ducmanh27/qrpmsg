//
// Created by manhpd9 on 14/10/2025.
//

#include "rpmsglinuxhelper.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/rpmsg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#if defined QRPMSG_DEBUG
#include <QDebug>
#endif
#include <cstring>
int RPMsgLinuxHelper::app_rpmsg_create_ept(int rpfd, struct rpmsg_endpoint_info *eptinfo) {
    int ret;

    ret = ioctl(rpfd, RPMSG_CREATE_EPT_IOCTL, eptinfo);
    if (ret)
        perror("Failed to create endpoint.\n");
    return ret;
}

char * RPMsgLinuxHelper::get_rpmsg_ept_dev_name(const char *rpmsg_char_name,
                                               const char *ept_name,
                                               char *ept_dev_name) {
    char sys_rpmsg_ept_name_path[64];
    std::string svc_name;
    char *sys_rpmsg_path = "/sys/class/rpmsg";
    FILE *fp;
    int i;
    int ept_name_len = strlen(ept_name);
    
    for (i = 0; i < 128; i++) {
        sprintf(sys_rpmsg_ept_name_path, "%s/%s/rpmsg%d/name",
                sys_rpmsg_path, rpmsg_char_name, i);
#if defined QRPMSG_DEBUG
        qDebug("checking %s\n", sys_rpmsg_ept_name_path);
#endif
        if (access(sys_rpmsg_ept_name_path, F_OK) < 0)
            continue;
        fp = fopen(sys_rpmsg_ept_name_path, "r");
        if (!fp) {
#if defined QRPMSG_DEBUG
            qDebug("failed to open %s\n", sys_rpmsg_ept_name_path);
#endif
            continue;
        }
        char buffer[64];
        if (fgets(buffer, sizeof(buffer), fp)) {
            svc_name = buffer;
            // Loại bỏ ký tự newline nếu có
            if (!svc_name.empty() && svc_name.back() == '\n') {
                svc_name.pop_back();
            }
        }
        fclose(fp);
        if (ept_name_len != svc_name.size())
            continue;
        if (!strncmp(svc_name.c_str(), ept_name, ept_name_len)) {
            sprintf(ept_dev_name, "rpmsg%d", i);
            return ept_dev_name;
        }
    }
#if defined QRPMSG_DEBUG
    qDebug("Not able to RPMsg endpoint file for %s:%s.\n",
                  rpmsg_char_name, ept_name);
#endif
    return NULL;
}

int RPMsgLinuxHelper::bind_rpmsg_chrdev(const char *rpmsg_dev_name) {
    char fpath[256];
    const char *rpmsg_chdrv = "rpmsg_chrdev";
    char drv_override[64] = {0};
    int fd;
    int ret;

    /* rpmsg dev overrides path */
    sprintf(fpath, "%s/devices/%s/driver_override",
        RPMSG_BUS_SYS, rpmsg_dev_name);
#if defined QRPMSG_DEBUG
    qDebug("open %s\n", fpath);
#endif
    fd = open(fpath, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s, %s\n",
            fpath, strerror(errno));
        return -EINVAL;
    }

    ret = read(fd, drv_override, sizeof(drv_override));
    if (ret < 0) {
        fprintf(stderr, "Failed to read %s (%s)\n",
            fpath, strerror(errno));
        close(fd);
        return ret;
    }
#if defined QRPMSG_DEBUG
    qDebug("current drv override = %s\n", drv_override);
#endif

    /*
     * Check driver override. If "rpmsg_chrdev" string is
     * found, then don't attempt to bind. If null string is found,
     * then no driver is bound, and attempt to bind rpmsg char driver.
     * Any other case, fail binding driver, as device is busy.
     */
    if (strncmp(drv_override, rpmsg_chdrv, strlen(rpmsg_chdrv)) == 0) {
        close(fd);
        return 0;
    } else if (strncmp(drv_override, "(null)", strlen("(null)")) != 0) {
        fprintf(stderr, "error: device %s is busy, drv bind=%s\n",
               rpmsg_dev_name, drv_override);
        close(fd);
        return -EBUSY;
    }

    ret = write(fd, rpmsg_chdrv, strlen(rpmsg_chdrv) + 1);
    if (ret < 0) {
        fprintf(stderr, "Failed to write %s to %s, %s\n",
            rpmsg_chdrv, fpath, strerror(errno));
        close(fd);
        return -EINVAL;
    }
    close(fd);

    /* bind the rpmsg device to rpmsg char driver */
    sprintf(fpath, "%s/drivers/%s/bind", RPMSG_BUS_SYS, rpmsg_chdrv);
    fd = open(fpath, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s, %s\n",
            fpath, strerror(errno));
        return -EINVAL;
    }
#if defined QRPMSG_DEBUG
    qDebug("write %s to %s\n", rpmsg_dev_name, fpath);
#endif
    ret = write(fd, rpmsg_dev_name, strlen(rpmsg_dev_name) + 1);
    if (ret < 0) {
        fprintf(stderr, "Failed to write %s to %s, %s\n",
            rpmsg_dev_name, fpath, strerror(errno));
        close(fd);
        return -EINVAL;
    }
    close(fd);
    return 0;
}

int RPMsgLinuxHelper::get_rpmsg_chrdev_fd(const char *rpmsg_dev_name, char *rpmsg_ctrl_name) {
    char dpath[2*NAME_MAX];
    DIR *dir;
    struct dirent *ent;
    int fd;

    sprintf(dpath, "%s/devices/%s/rpmsg", RPMSG_BUS_SYS, rpmsg_dev_name);
#if defined QRPMSG_DEBUG
    qDebug("opendir %s\n", dpath);
#endif
    dir = opendir(dpath);
    if (dir == NULL) {
        fprintf(stderr, "opendir %s, %s\n", dpath, strerror(errno));
        return -EINVAL;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (!strncmp(ent->d_name, "rpmsg_ctrl", 10)) {
            sprintf(dpath, "/dev/%s", ent->d_name);
            closedir(dir);
#if defined QRPMSG_DEBUG
            qDebug("open %s\n", dpath);
#endif
            fd = open(dpath, O_RDWR | O_NONBLOCK);
            if (fd < 0) {
                fprintf(stderr, "open %s, %s\n",
                    dpath, strerror(errno));
                return fd;
            }
            sprintf(rpmsg_ctrl_name, "%s", ent->d_name);
            return fd;
        }
    }

    fprintf(stderr, "No rpmsg_ctrl file found in %s\n", dpath);
    closedir(dir);
    return -EINVAL;
}

int RPMsgLinuxHelper::lookup_channel(char *out, struct rpmsg_endpoint_info *pep) {
    char dpath[] = RPMSG_BUS_SYS "/devices";
    struct dirent *ent;
    DIR *dir = opendir(dpath);
    if (dir == NULL) {
        fprintf(stderr, "opendir %s, %s\n", dpath, strerror(errno));
        return -EINVAL;
    }

    size_t name_len = strlen(pep->name);

    // Scan tất cả devices trong /sys/bus/rpmsg/devices/
    while ((ent = readdir(dir)) != NULL) {
        // Tìm vị trí của pep->name trong d_name
        char *pos = strstr(ent->d_name, pep->name);

        if (pos != NULL) {
            // Kiểm tra ký tự sau pep->name phải là '.' hoặc '\0'
            char next_char = pos[name_len];
            if (next_char == '.' || next_char == '\0') {
                strncpy(out, ent->d_name, NAME_MAX);
                set_src_dst(out, pep); // Parse destination address
#if defined QRPMSG_DEBUG
                qDebug("using dev file: %s\n", out);
#endif
                closedir(dir);
                return 0;
            }
        }
    }
    closedir(dir);
    fprintf(stderr, "No dev file for %s in %s\n", pep->name, dpath);
    return -EINVAL;
}

void RPMsgLinuxHelper::set_src_dst(char *out, struct rpmsg_endpoint_info *pep) {
    long dst = 0;
    char *lastdot = strrchr(out, '.');

    if (lastdot == NULL)
        return;
    dst = strtol(lastdot + 1, NULL, 10);
    if ((errno == ERANGE && (dst == LONG_MAX || dst == LONG_MIN))
        || (errno != 0 && dst == 0)) {
        return;
        }
    pep->dst = (unsigned int)dst;
}
