import subprocess
import sys
import time
import os
import signal
import glob

output_dir = [
    '',
    './testcases/t1/output',
    './testcases/t2/output',
    './testcases/t3/output',
]

file_list_t1 = [
    ['testcases/t1/coordinator.tl','./testcases/t1/output/coordinator.output'],
    ['testcases/t1/server.tl','./testcases/t1/output/server.output'],
    ['testcases/t1/user1.tl','./testcases/t1/output/user1.output'],
    ['testcases/t1/user4.tl','./testcases/t1/output/user4.output'],
]

file_list_t2 = [
    ['testcases/t2/coordinator.tl','./testcases/t2/output/coordinator.output'],
    ['testcases/t2/server_c1.tl','./testcases/t2/output/server_c1.output'],
    ['testcases/t2/server_c2.tl','./testcases/t2/output/server_c2.output'],
    ['testcases/t2/server_c3.tl','./testcases/t2/output/server_c3.output'],
    ['testcases/t2/sync.tl','./testcases/t2/output/sync.output'],
    ['testcases/t2/user1.tl','./testcases/t2/output/user1.output'],
    ['testcases/t2/user2.tl','./testcases/t2/output/user2.output'],
    ['testcases/t2/user3.tl','./testcases/t2/output/user3.output'],
]

file_list_t3 = [
    ['testcases/t3/coordinator.tl',output_dir[3]+'/coordinator.output'],
    ['testcases/t3/server_c1.tl',output_dir[3]+'/server_c1.output'],
    ['testcases/t3/server_c2_s1.tl',output_dir[3]+'/server_c2_s1.output'],
    ['testcases/t3/server_c2_s2.tl',output_dir[3]+'/server_c2_s2.output'],
    ['testcases/t3/server_c3.tl',output_dir[3]+'/server_c3.output'],
    ['testcases/t3/sync.tl',output_dir[3]+'/sync.output'],
    ['testcases/t3/user1.tl',output_dir[3]+'/user1.output'],
    ['testcases/t3/user2.tl',output_dir[3]+'/user2.output'],
    ['testcases/t3/user3.tl',output_dir[3]+'/user3.output'],
    ['testcases/t3/user5.tl',output_dir[3]+'/user5.output'],
]


def parse_files(filelist):
    all_files_sections = []

    for filepath in filelist:
        with open(filepath[0], 'r') as file:
            content = file.read()

            sections = content.strip().split('\n\n')
            all_files_sections.append(sections)

    return all_files_sections

def run_section(idx,commands,processes,output):
    if commands[0].startswith("NULL") :
        print("NULL")
        return

    elif commands[0].startswith("SLEEP") :
        # print("SLEEP : ",commands[1])
        # time.sleep(float(commands[1]))
        print("SLEEP : ",30)
        time.sleep(30)
        if commands[1] == "10000":
            time.sleep(10000)
    
    elif commands[0].startswith("RUN") :
        for command in commands[1:]:
            print("RUN : ",command)
            sections = command.strip().split(' ')
            with open(output, 'a+') as output_file:
                subprocess.Popen(sections, stdin=subprocess.PIPE, stdout=output_file, stderr=output_file)
                time.sleep(1)

    elif commands[0].startswith("INTER_RUN") :
        for command in commands[1:]:
            print("RUN : ",command)
            sections = command.strip().split(' ')
            with open(output, 'a+') as output_file:
                p = subprocess.Popen(sections, stdin=subprocess.PIPE,stdout=output_file, stderr=output_file,bufsize=0,universal_newlines=True)
                processes[idx] = p
                time.sleep(1)

    elif commands[0].startswith("SEND") :
        process = processes[idx]
        for command in commands[1:]:
            print("SEND : ",command)
            command = command.strip()
            # stdout_data, stderr_data = process.communicate(input=command.encode())
            command = command+"\n"
            process.stdin.write(command)
            process.stdin.flush()
            
            time.sleep(0.5)

    elif commands[0].startswith("KILL") :
        print("KILL")
        proc = processes[idx]

        proc.stdin.flush()

        proc.terminate()
        proc.wait()
        # os.killpg(proc.pid, signal.SIGTERM) 


def run_test(filelist):
    all_files_sections = parse_files(filelist)
    processes  = [0] * (len(all_files_sections)+1)
   
    max_sections_length = max(len(sections) for sections in all_files_sections)

    for i in range(max_sections_length):
        idx = 0
        print("====================== ",i," ========================")
        for file_sections in all_files_sections:
            if i < len(file_sections):
                print("-----------")
                sections = file_sections[i].strip().split('\n')
                run_section(idx,sections,processes,filelist[idx][1])

            idx += 1
        print("===================================================\n")


def empty_dir(dir):
    files = glob.glob(os.path.join(dir, '*'))

    for file in files:
        try:
            os.remove(file)
        except OSError as e:
            print(f"Error: {e.filename} - {e.strerror}")

if __name__ == "__main__":
    testcase = first_argument = sys.argv[1]

    subprocess.Popen(['rm *.txt'],shell=True)   
    time.sleep(1)
    
    if testcase == 't1' :
        empty_dir(output_dir[1])
        run_test(file_list_t1)

    if testcase == 't2' :
        empty_dir(output_dir[2])
        run_test(file_list_t2)

    if testcase == 't3' :
        empty_dir(output_dir[3])
        run_test(file_list_t3)
    

