/**
 * @file ks_features/syscall_list.h
 * @author Vyacheslav Cherkashin: SWAP ks_features implement
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Syscalls list.
 */


#ifndef _SYSCALL_LIST_H
#define _SYSCALL_LIST_H

#define SYSCALL_LIST \
	X(accept4, dpdd) \
	X(accept, dpd) \
	X(access, sd) \
	X(acct, s) \
	X(bind, dpd) \
	X(chdir, s) \
	X(chmod, sd) \
/* TODO: X(chown16, sdd) */ \
	X(chown, sdd) \
	X(chroot, s) \
	X(clone, ddddd) \
	X(connect, dpd) \
	X(creat, sd) \
	X(dup3, ddd) \
	X(epoll_create1, d) \
	X(epoll_ctl, dddp) \
	X(epoll_pwait, dpddpx) \
	X(epoll_wait, dpdd) \
	X(eventfd2, dd) \
	X(eventfd, d) \
	X(execve, spp) \
	X(exit, d) \
	X(exit_group, d) \
	X(faccessat, dsd) \
/* TODO: X(fadvise64_64, dxxd) */ \
	X(fallocate, ddxx) \
	X(fanotify_init, dd) \
	X(fanotify_mark, ddxds) \
	X(fchmodat, dsd) \
	X(fchownat, dsddd) \
	X(fgetxattr, dspx) \
	X(flistxattr, dpx) \
	X(fork, /* empty */) \
	X(fremovexattr, ds) \
	X(fstat64, xp) \
	X(ftruncate64, dx) \
	X(futimesat, dsp) \
	X(getcwd, px) \
	X(getpeername, dpd) \
	X(getsockname, dpd) \
	X(getsockopt, dddpd) \
	X(getxattr, sspx) \
	X(inotify_add_watch, dsd) \
	X(inotify_init, /* empty */) \
	X(inotify_init1, d) \
	X(inotify_rm_watch, dd) \
/* TODO: X(ipc, ddxxpx) */\
	X(kill, dd) \
	X(linkat, dsdsd) \
	X(link, ss) \
	X(listen, dd) \
	X(listxattr, spx) \
	X(lstat64, sp) \
/* TODO: X(lstat, sp) */ \
	X(mkdirat, dsd) \
	X(mkdir, sd) \
	X(mknodat, dsdd) \
	X(mknod, sdd) \
/* TODO: X(mmap_pgoff, xxxxxx) */ \
	X(mount, pppxp) \
	X(msgctl, ddp) \
	X(msgget, dd) \
	X(msgrcv, dpxxd) \
	X(msgsnd, dpxd) \
	X(name_to_handle_at, dspdd) \
/* TODO: X(newfstatat, dspd) */ \
/* TODO: X(old_mmap, p) */ \
	X(openat, dsdd) \
	X(open_by_handle_at, dpd) \
	X(open, sdd) \
	X(pause, /* empty */) \
	X(pipe2, dd) \
	X(ppoll, pdpp) \
	X(pread64, dpxx) \
	X(preadv, xpxxx) \
	X(pselect6, dxxxpp) \
	X(pwrite64, dsxx) \
	X(pwritev, xpxxx) \
	X(readlinkat, dspd) \
	X(readlink, spd) \
	X(recv, dpxd) \
	X(recvfrom, dpxdpd) \
	X(recvmmsg, dpddp) \
	X(recvmsg, dpd) \
	X(removexattr, ss) \
	X(renameat, dsds) \
	X(rename, ss) \
	X(rmdir, s) \
	X(rt_sigaction, dpp) \
	X(rt_sigprocmask, dppx) \
	X(rt_sigsuspend, px) \
	X(rt_sigtimedwait, pppx) \
	X(rt_tgsigqueueinfo, dddp) \
	X(semctl, dddx) \
	X(semget, ddd) \
	X(semop, dpd) \
	X(semtimedop, dpdp) \
	X(send, dpxd) \
	X(sendfile64, ddlxx) \
	X(sendfile, ddxx) \
	X(sendmmsg, dpdd) \
	X(sendmsg, dpd) \
	X(sendto, dpxdpd) \
	X(setns, dd) \
	X(setsockopt, dddpd) \
	X(setxattr, sspxd) \
	X(shmat, dpd) \
	X(shmctl, ddp) \
	X(shmdt, p) \
	X(shmget, dxd) \
	X(shutdown, dd) \
	X(sigaction, dpp) \
/* TODO: X(sigaltstack, pp) */ \
/* TODO: X(signal, dp) */ \
	X(signalfd4, dpxd) \
	X(signalfd, dpx) \
	X(sigpending, p) \
	X(sigprocmask, dpp) \
/* TODO: X(sigsuspend, ddp) */ \
/* TODO: X(sigsuspend, p) */ \
/* TODO: X(socketcall, dx) */\
	X(socket, ddd) \
	X(socketpair, dddd) \
	X(splice, dxdxxd) \
	X(stat64, sp) \
	X(statfs64, sxp) \
	X(statfs, sp) \
/* TODO: X(stat, sp) */ \
	X(swapoff, s) \
	X(swapon, sd) \
	X(symlinkat, sds) \
	X(symlink, ss) \
	X(syncfs, d) \
	X(tee, ddxd) \
	X(tgkill, ddd) \
	X(timerfd_create, dd) \
	X(timerfd_gettime, dp) \
	X(timerfd_settime, ddpp) \
	X(truncate64, sx) \
	X(truncate, sx) \
/* TODO: X(umount, pd) */\
	X(unlinkat, dsd) \
	X(unlink, s) \
	X(unshare, x) \
	X(uselib, s) \
	X(utimensat, dspd) \
/* TODO: X(utime, pp) */\
	X(utimes, pp) \
	X(vfork, /* empty */) \
	X(vmsplice, dpxd) \
	X(wait4, dddp) \
	X(waitid, ddpdp)
/* TODO: X(waitpid, ddd) */

#endif /* _SYSCALL_LIST_H */
