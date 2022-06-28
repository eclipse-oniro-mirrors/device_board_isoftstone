# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


pack_config_file=.packconfig

function mk_info()
{
	echo -e "\033[47;30mINFO: $*\033[0m"
}

function mkpack()
{
	mk_info "packing firmware ..."
	echo $@

	source ${pack_config_file}
	echo ${LICHEE_CHIP_CONFIG_DIR}
	local PACK_PLATFORM=$LICHEE_PLATFORM
	if [ "x${LICHEE_PLATFORM}" != "xandroid" -a \
	     "x${LICHEE_PLATFORM}" != "x3rd" ]; then
		PACK_PLATFORM=$LICHEE_LINUX_DEV
		(cd ${LICHEE_TOP_DIR}/scripts && \
		./pack -i ${LICHEE_IC} -c ${LICHEE_CHIP} -p ${PACK_PLATFORM} -b ${LICHEE_BOARD} -k ${LICHEE_KERN_VER} -n ${LICHEE_FLASH} $@)
	elif [ "x${LICHEE_PLATFORM}" == "x3rd" ]; then
		PACK_PLATFORM=$LICHEE_LINUX_DEV
		(cd ${LICHEE_TOP_DIR}/scripts && \
		./pack -i ${LICHEE_IC} -c ${LICHEE_CHIP} -p ${PACK_PLATFORM} -b ${LICHEE_BOARD} -k ${LICHEE_KERN_VER} -n ${LICHEE_FLASH} $@)
	fi

}

mkpack
[ $? -ne 0 ] && return 1
