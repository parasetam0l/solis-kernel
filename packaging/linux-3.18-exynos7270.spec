%define CHIPSET exynos7270
%define KERNEL_VERSION 3.18
%define ARM arm64
%define IMAGE Image
%define DZIMAGE dzImage

Name: linux-%{KERNEL_VERSION}-%{CHIPSET}
Summary: The Linux Kernel
Version: Tizen_exynos7270_20170920_1_6f0d973b
Release: 1
License: GPL-2.0
Group: System/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root
Provides: linux-%{KERNEL_VERSION}
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

BuildRequires: lzop
BuildRequires: binutils-devel
BuildRequires: module-init-tools
BuildRequires: python
BuildRequires: gcc
BuildRequires: bash
BuildRequires: system-tools
BuildRequires: sec-product-features
BuildRequires: bc
BuildRequires: build-info
BuildRequires: cross-aarch64-gcc-x86-arm
BuildRequires: cross-aarch64-binutils-x86-arm
ExclusiveArch: %arm

%description
The Linux Kernel, the operating system core itself

%if "%{?sec_product_feature_kernel_defconfig}" == "undefined"
%define MODEL tizen_solis
%else
%define MODEL tizen_%{?sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_system_carrier_type}" == "undefined"
%define CARRIER ""
%else
%define CARRIER %{?sec_product_feature_system_carrier_type}
%endif

%if "%{?sec_product_feature_system_region_name}" == "undefined"
%define REGION ""
%else
%define REGION %{?sec_product_feature_system_region_name}
%endif

%if "%{?sec_product_feature_system_operator_name}" == "undefined"
%define OPERATOR ""
%else
%define OPERATOR %{?sec_product_feature_system_operator_name}
%endif

%if "%{?sec_product_feature_showcase_enable}" == "1"
%define SHOWCASE ifa
%else
%define SHOWCASE na
%endif


%package -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
License: GPL-2.0
Summary: Linux support headers for userspace development
Group: System/Kernel
Requires(post): coreutils

%files -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
/boot/kernel/mod_%{MODEL}
/boot/kernel/kernel-%{MODEL}/%{DZIMAGE}
/boot/kernel/kernel-%{MODEL}/%{DZIMAGE}-recovery

%post -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
mv /boot/kernel/mod_%{MODEL}/lib/modules/* /lib/modules/.
mv /boot/kernel/kernel-%{MODEL}/%{DZIMAGE} /boot/kernel/.
mv /boot/kernel/kernel-%{MODEL}/%{DZIMAGE}-recovery /boot/kernel/.

%description -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
This package provides the %{CHIPSET}_eur linux kernel image & module.img.

%package -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}-debuginfo
License: GPL-2.0
Summary: Linux support debug symbol
Group: System/Kernel

%files -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}-debuginfo
/boot/kernel/mod_%{MODEL}
/boot/kernel/kernel-%{MODEL}

%description -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}-debuginfo
This package provides the %{CHIPSET}_eur linux kernel's debugging files.

%package -n kernel-headers-%{KERNEL_VERSION}-%{CHIPSET}
License: GPL-2.0
Summary: Linux support headers for userspace development
Group: System/Kernel
Provides: linux-glibc-devel, kernel-headers-tizen-dev
Obsoletes: linux-glibc-devel

%description -n kernel-headers-%{KERNEL_VERSION}-%{CHIPSET}
This package provides userspaces headers from the Linux kernel. These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%package -n kernel-devel-%{KERNEL_VERSION}-%{CHIPSET}
License: GPL-2.0
Summary: Linux support kernel map and etc for other package
Group: System/Kernel
Provides: kernel-devel-tizen-dev

%description -n kernel-devel-%{KERNEL_VERSION}-%{CHIPSET}
This package provides kernel map and etc information.

%package -n linux-kernel-license
License: GPL-2.0
Summary: Linux support kernel license file
Group: System/Kernel

%description -n linux-kernel-license
This package provides kernel license file.

%prep
%setup -q

%build
export PATH=/opt/cross/bin:$PATH
export CROSS_COMPILE=aarch64-tizen-linux-gnu-
export OBS_BUILD_VERSION=%{sec_build_option_version}
export OBS_BUILD_COMMIT=`echo %{vcs}|tr '#' ' '|awk '{print $2}'|head -c 7`

%if 0%{?tizen_build_binary_release_type_eng}
%define RELEASE eng
%else
%define RELEASE usr
%endif

mkdir -p %{_builddir}/mod_%{MODEL}
make distclean

./release_obs.sh %{RELEASE} %{MODEL} %{CARRIER} %{REGION} %{OPERATOR} %{SHOWCASE}

cp -f arch/%{ARM}/boot/%{IMAGE} %{_builddir}/%{IMAGE}.%{MODEL}
cp -f arch/%{ARM}/boot/merged-dtb %{_builddir}/merged-dtb.%{MODEL}
cp -f arch/%{ARM}/boot/%{DZIMAGE} %{_builddir}/%{DZIMAGE}.%{MODEL}
cp -f arch/%{ARM}/boot/%{DZIMAGE} %{_builddir}/%{DZIMAGE}-recovery.%{MODEL}
cp -f System.map %{_builddir}/System.map.%{MODEL}
cp -f .config %{_builddir}/config.%{MODEL}
cp -f vmlinux %{_builddir}/vmlinux.%{MODEL}

#remove all changed source codes for next build
cd %_builddir
rm -rf %{name}-%{version}
/bin/tar -xf %{SOURCE0}
cd %{name}-%{version}

%install
mkdir -p %{buildroot}/usr
make mrproper
make headers_check
make headers_install INSTALL_HDR_PATH=%{buildroot}/usr

find  %{buildroot}/usr/include -name ".install" | xargs rm -f
find  %{buildroot}/usr/include -name "..install.cmd" | xargs rm -f
rm -rf %{buildroot}/usr/include/scsi
rm -f %{buildroot}/usr/include/asm*/atomic.h
rm -f %{buildroot}/usr/include/asm*/io.h

mkdir -p %{buildroot}/usr/share/license
cp -vf COPYING %{buildroot}/usr/share/license/linux-kernel

mkdir -p %{buildroot}/boot/kernel/kernel-%{MODEL}
mkdir -p %{buildroot}/boot/kernel/license-%{MODEL}

mv %_builddir/mod_%{MODEL} %{buildroot}/boot/kernel/mod_%{MODEL}

mv %_builddir/%{IMAGE}.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/%{IMAGE}
mv %_builddir/merged-dtb.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/merged-dtb
mv %_builddir/%{DZIMAGE}.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/%{DZIMAGE}
mv %_builddir/%{DZIMAGE}-recovery.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/%{DZIMAGE}-recovery

mv %_builddir/System.map.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/System.map
mv %_builddir/config.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/config
mv %_builddir/vmlinux.%{MODEL} %{buildroot}/boot/kernel/kernel-%{MODEL}/vmlinux

find %{buildroot}/boot/kernel/ -name 'System.map' > develfiles.pre # for secure storage
find %{buildroot}/boot/kernel/ -name 'vmlinux' >> develfiles.pre   # for TIMA
find %{buildroot}/boot/kernel/ -name '*.ko' >> develfiles.pre      # for TIMA
find %{buildroot}/boot/kernel/ -name '*%{IMAGE}' >> develfiles.pre   # for Trusted Boot
cat develfiles.pre | sed -e "s#%{buildroot}##g" | uniq | sort > develfiles

%clean
rm -rf %_builddir

%files -n kernel-headers-%{KERNEL_VERSION}-%{CHIPSET}
/usr/include/*

%files -n linux-kernel-license
/usr/share/license/*

%files -n kernel-devel-%{KERNEL_VERSION}-%{CHIPSET} -f develfiles
