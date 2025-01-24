nomalloc = environment({'MALLOC_PERTURB_': '0'})

bash = find_program('bash')
testTimeout = 600

# Tests for copyrighted game roms (only for local testing or own CI runners)
protectedSimpleTestSet = [ 
  'doom1.e1.uvmax',
  'doom1.e2.uvmax',
  'doom1.e3.uvmax',
  'doom1.e4.uvmax',
  'doom2.full.uvmax',
  'plutonia.full.uvmax',
  'tnt.full.uvmax',
]

protectedRerecordTestSet = [ 
  'doom2.m01.uvspeed',
  'doom2.m15.uvspeed',
  'doom2.m21.uvspeed',
  'doom2.m30.uvspeed',
]

# Tests for free wads (for public cloud testing)
freeRerecordTestSet = [
    'freedoom011.e1m1.reduced',
    'freedoom011.e1m1.easy',
    'freedoom011.e1m1.pacifist',
    'freedoom011.e1m2.easy',
    'freedoom011.e1m2.uvspeed',
    'freedoom011.e1m3.easy',
    'freedoom011.e1m4.easy',
    'freedoom011.e1m5.easy',
    'freedoom011.e1m6.easy',
    'freedoom011.e1m7.easy',
    'freedoom011.e1m8.easy',
    'freedoom011.e1m8.uvspeed',
    'freedoom011.e1m9.easy',
    'freedoom011.e2m1.easy',
    'freedoom011.e2m2.easy',
    'freedoom011.e2m3.easy',
    'freedoom011.e2m4.easy',
    'freedoom011.e2m5.easy',
    'freedoom011.e2m6.easy',
    'freedoom011.e2m7.easy',
    'freedoom011.e2m8.easy',
    'freedoom011.e3m6.uvspeed',
    'freedoom011.m01.pacifist',
    'freedoom011.m02.uvspeed',
    'freedoom011.m05.uvspeed',
    'freedoom011.m31.pacifist',
    'freedoom011.m31.uvmax',
]

freeSimpleTestSet = [
]

# Creating test set based on whether copyrighted roms are to be used
simpleTestSet = freeSimpleTestSet
rerecordTestSet = freeRerecordTestSet
if get_option('onlyFree') == false
 simpleTestSet += protectedSimpleTestSet
 rerecordTestSet += protectedRerecordTestSet
endif

# Adding tests to the suite
foreach testFile : simpleTestSet
  testSuite = testFile.split('.')[0]
  testName = testFile.split('.')[1] + '.' + testFile.split('.')[2]
  test(testName,
       bash,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ 'run_test_simple.sh', baseTester.path(),  newTester.path(), testFile + '.test', testFile + '.sol' ],
       suite : [ testSuite ])
endforeach

foreach testFile : rerecordTestSet
  testSuite = testFile.split('.')[0]
  testName = testFile.split('.')[1] + '.' + testFile.split('.')[2]
  test(testName,
       bash,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ 'run_test_rerecord.sh', baseTester.path(),  newTester.path(), testFile + '.test', testFile + '.sol' ],
       suite : [ testSuite ])
endforeach

# Parallel testing
foreach testFile : rerecordTestSet
  testSuite = testFile.split('.')[0]
  testName = testFile.split('.')[1] + '.' + testFile.split('.')[2] + '.' + 'parallel'
  test(testName,
       pTester,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ testFile + '.test', testFile + '.sol', '--cycleType', 'Rerecord', '--rerecordDepth', '16' ],
       suite : [ testSuite ])
endforeach