"""
Copyright (c) 2022 VYAGOO TECHNOLOGY Co., Ltd.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import os
import patch
import sys
from subprocess import Popen, PIPE

def clean_code(patch_set):
  print("----------- clean code ----------")
  for git_path in patch_set.keys():
    print(git_path)
    p = Popen("git restore .", shell=True, stdout=PIPE, cwd=git_path)
    p.wait()
    p = Popen("git clean -fd", shell=True, stdout=PIPE, cwd=git_path)
    p.wait()

def patch():
  print("----------patch code to harmony--------")
  patch_set = get_file_set(patch_path)
  if not patch_set:
    print("---- [fasle] no patch to do ----")
    sys.exit(1)
  #检查每个补丁路径是否正确
  if not check_gits_exit(patch_set.keys()):
    return False
  clean_code(patch_set)
  patch_code_to_harmony(patch_set)
  print("---- [success] patch code to harmony -----")

def patch_code_to_harmony(patch_set):
  for git_path in patch_set.keys():
    if not patch_set[git_path]:
      continue
    patch_path = patch_set[git_path]
    p = Popen(f"git apply {patch_path}/*.patch ",shell=True, stdout=PIPE, cwd=git_path)
    p.wait()

def push_source():
  print("----------push source to harmony-------")
  source_set = get_file_set(code_source_path)
  if not source_set:
    print("---- [false] no resource ----")
    return False
  push_source_to_harmony(source_set)

def push_source_to_harmony(source_set):
  for git_path in source_set.keys():
    if not source_set[git_path]:
      continue
    source_path = source_set[git_path]
    p = Popen(f"cp -a {source_path}/* {git_path}/", shell=True, stdout=PIPE)
    p.wait()
    print("---- [success] push source to harmony ----")

def check_gits_exit(git_paths: list):
  for path in git_paths:
    git_path = os.path.join(path, ".git")
    if not os.path.isdir(git_path):
      print(f"---- [false] not found {git_path} ----")
      return False
  return True

def get_file_set(path):
  file_set = {}
  if not os.path.isdir(path):
    print(f"not fount {path}, please check")
    sys.exit(1)
  git_dir_list = []
  for root, dirs, files in os.walk(path):
    for dir in dirs:
      dir_path = os.path.join(root, dir)
      if os.path.isdir(dir_path):
        git_path = os.path.join(root_path, str(dir).replace("-", "/"))
        file_set[git_path] = dir_path
    break
  return file_set

def copy_new_file(src_path, dest_path):
  if not os.path.isdir(dest_path):
    print(f"[error] not found {dest_path}, please cheak!")
    return false
  if not os.path.exists(src_path):
    print(f"[error] not fount {src_path}, please check!")
    return false
  p = Popen(f"cp -a {src_path} {dest_path}/", shell=True, stdout=PIPE)
  p.wait()

def copy_file():
  src_device_ic_json = os.path.join(file_source_path, 'productdefine', 'device', json_name)
  dest_device_ic_json = os.path.join(root_path, 'productdefine/common/device')
  copy_new_file(src_device_ic_json, dest_device_ic_json)

  src_product_ic_json = os.path.join(file_source_path, 'productdefine', 'product', json_name)
  dest_product_ic_json = os.path.join(root_path, 'productdefine/common/products')
  copy_new_file(src_product_ic_json, dest_product_ic_json)
  print("---- [success] copy file to harmony ----")

if __name__ == "__main__":

  support_platforms = ["T507", "R818"]
  curent_path = os.getcwd()
  root_path = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(curent_path))))
  allwinner_path = os.path.join(root_path, 'device', "soc", 'allwinner')


  if len(sys.argv) <= 1:
    print("[error] please selete ic only support 'T507' or 'R818' now")
    print("[sample] 'python3 patch.py R818'")
    sys.exit(1)
  current_ic = str(sys.argv[-1]).upper()
  print(f"ic name is {current_ic}")
  if current_ic not in support_platforms:
    print(f"[error] {current_ic} not support")
    sys.exit(1)

  ic_path = os.path.join(allwinner_path, current_ic)
  patch_path = os.path.join(ic_path, 'patches', 'harmony')
 # code_source_path = os.path.join(source_path, 'code')
  file_source_path = os.path.join(ic_path, 'patches', 'file')
  json_name = current_ic + '.json'
  patch()
  #push_source()
  copy_file()



