# This file is part of OpenPanel - The Open Source Control Panel
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/
%define version 0.9.4

%define libpath /usr/lib
%ifarch x86_64
  %define libpath /usr/lib64
%endif

Summary: cli
Name: openpanel-cli
Version: %version
Release: 1
License: GPLv2
Group: Development
Source: http://packages.openpanel.com/archive/openpanel-cli-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-buildroot
Requires: openpanel-core >= 0.8.0
Requires: libgrace
Requires: libgrace-ssl

%description
cli
the commandline interface
%prep
%setup -q -n openpanel-cli-%version

%build
BUILD_ROOT=$RPM_BUILD_ROOT
./configure
make

%install
BUILD_ROOT=$RPM_BUILD_ROOT
rm -rf ${BUILD_ROOT}
mkdir -p ${BUILD_ROOT}/usr/bin ${BUILD_ROOT}/usr/share/man/man1
install -m 755 opencli ${BUILD_ROOT}/usr/bin
gzip -c < opencli.1 > ${BUILD_ROOT}/usr/share/man/man1/opencli.1.gz

%post

%files
%defattr(-,root,root)
/
