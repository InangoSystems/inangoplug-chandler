#!/bin/sh
################################################################################
#
#  Copyright 2020-2021 Inango Systems Ltd.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
################################################################################

# ./utils/run.sh _bin/chandler -c utils/chandler.conf

CHANDLER_OVS_RUNDIR="/usr/local/var/run/openvswitch"

sudo env CHANDLER_CHECK_INTERVAL=60000 CHANDLER_OVS_RUNDIR="$CHANDLER_OVS_RUNDIR" $@
