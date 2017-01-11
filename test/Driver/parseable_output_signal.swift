// RUN: not %swiftc_driver_plain -Xfrontend -debug-crash-immediately -parseable-output -o %t.out %s 2>&1 | %FileCheck %s

// CHECK: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "began",
// CHECK-NEXT:   "name": "compile",
// CHECK-NEXT:   "command": "{{.*}}/swift{{c?}} -frontend -c -primary-file {{.*}}/parseable_output_signal.swift {{.*}} -o {{.*}}/parseable_output_signal-[[OUTPUT:.*]].o",
// CHECK-NEXT:   "inputs": [
// CHECK-NEXT:     "{{.*}}/parseable_output_signal.swift"
// CHECK-NEXT:   ],
// CHECK-NEXT:   "outputs": [
// CHECK-NEXT:     {
// CHECK-NEXT:       "type": "object",
// CHECK-NEXT:       "path": "{{.*}}/parseable_output_signal-[[OUTPUT]].o"
// CHECK-NEXT:     }
// CHECK-NEXT:  ],
// CHECK-NEXT:  "pid": {{[1-9][0-9]*}}
// CHECK-NEXT: }
// CHECK-NEXT: {{[1-9][0-9]*}}
// CHECK-NEXT: {
// CHECK-NEXT:   "kind": "signalled",
// CHECK-NEXT:   "name": "compile",
// CHECK-NEXT:   "pid": {{[1-9][0-9]*}},
// CHECK-NEXT:   "output": "{{.*}}Stack dump:\n0.\tProgram arguments: {{.*}}/swift -frontend -c -primary-file {{.*}}/parseable_output_signal.swift -target {{.*}} -debug-crash-immediately -module-name main -o {{.*}}/parseable_output_signal-[[OUTPUT:.*]].o \n",
// CHECK-NEXT:   "error-message": "Illegal instruction{{.*}}",
// CHECK-NEXT:   "signal": 4
// CHECK-NEXT: }
// CHECK-NEXT: <unknown>:0: error: unable to execute command: Illegal instruction{{.*}}
// CHECK-NEXT: <unknown>:0: error: compile command failed due to signal (use -v to see invocation)
