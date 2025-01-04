nomalloc = environment({'MALLOC_PERTURB_': '0'})

bash = find_program('bash')
testTimeout = 240

# Tests for copyrighted game roms (only for local testing or own CI runners)
protectedTestSet = [ 
  'princeOfPersia.playaround',
]

# Tests for open source free roms (for public cloud testing)
openSourceTestSet = [
  'airTrafficController.playaround',
  'aslimetravel.playaround',
]

# Creating test set based on whether copyrighted roms are to be used
testSet = openSourceTestSet
if get_option('onlyOpenSource') == false
 testSet += protectedTestSet
endif

# Adding tests to the suite
foreach testFile : testSet
  testSuite = testFile.split('.')[0]
  testName = testFile.split('.')[1]
  test(testName,
       bash,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ 'run_test.sh', baseTester.path(),  newTester.path(), testFile + '.test', testFile + '.sol' ],
       suite : [ testSuite ])
endforeach
