import subprocess
import sys
import time
import os
import signal

file_list_t1 = [
    ['testcases/t1/coordinator.tl','./testcases/t1/output/coordinator.output'],
    ['testcases/t1/server.tl','./testcases/t1/output/server.output'],
    ['testcases/t1/user1.tl','./testcases/t1/output/user1.output'],
    ['testcases/t1/user4.tl','./testcases/t1/output/user4.output'],
]

file_list_t2 = [
    ['testcases/t2/coordinator.tl','./testcases/t2/output/coordinator.output'],
    ['testcases/t2/server.tl','./testcases/t2/output/server.output'],
    ['testcases/t2/sync.tl','./testcases/t2/output/sync.output'],
    ['testcases/t2/user1.tl','./testcases/t2/output/user1.output'],
    ['testcases/t2/user2.tl','./testcases/t2/output/user2.output'],
    ['testcases/t2/user3.tl','./testcases/t2/output/user3.output'],
]


def parse_files(filelist):
    all_files_sections = []

    for filepath in filelist:
        with open(filepath[0], 'r') as file:
            content = file.read()

            # 以连续的\n分割sections
            sections = content.strip().split('\n\n')
            all_files_sections.append(sections)

    return all_files_sections

def run_section(idx,commands,processes,output):
    if commands[0].startswith("NULL") :
        print("NULL")
        return

    elif commands[0].startswith("SLEEP") :
        print("SLEEP : ",commands[1])
        time.sleep(float(commands[1]))
    
    elif commands[0].startswith("RUN") :
        for command in commands[1:]:
            print("RUN : ",command)
            sections = command.strip().split(' ')
            with open(output, 'w+') as output_file:
                subprocess.Popen(sections, stdin=subprocess.PIPE, stdout=output_file, stderr=output_file)
                time.sleep(1)

    elif commands[0].startswith("INTER_RUN") :
        for command in commands[1:]:
            print("RUN : ",command)
            sections = command.strip().split(' ')
            with open(output, 'w+') as output_file:
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
        proc = processes[idx]

        proc.stdin.flush()

        proc.terminate()
        proc.wait()
        os.killpg(proc.pid, signal.SIGTERM) 


def run_test(filelist):
    all_files_sections = parse_files(filelist)
    processes  = [0] * (len(all_files_sections)+1)
   
    max_sections_length = max(len(sections) for sections in all_files_sections)

    for i in range(max_sections_length):
        idx = 0
        for file_sections in all_files_sections:
            if i < len(file_sections):
                print("-------")
                sections = file_sections[i].strip().split('\n')
                run_section(idx,sections,processes,filelist[idx][1])

            idx += 1


if __name__ == "__main__":
    testcase = first_argument = sys.argv[1]


    subprocess.Popen(['rm *.txt'],shell=True)   
    time.sleep(1)

    if testcase == 't1' :
        run_test(file_list_t1)
    if testcase == 't2' :
        run_test(file_list_t2)
    

