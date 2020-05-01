/*
 * Copyright (c) 2007, Michael Feathers, James Grenning and Bas Vodde
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE EARLIER MENTIONED AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestMemoryAllocator.h"
#include "CppUTest/PlatformSpecificFunctions.h"
#include "CppUTest/TestTestingFixture.h"

TEST_GROUP(TestMemoryAllocatorTest)
{
    TestMemoryAllocator* allocator;
    GlobalMemoryAllocatorStash memoryAllocatorStash;

    void setup()
    {
        memoryAllocatorStash.save();
        allocator = NULLPTR;
    }

    void teardown()
    {
        delete allocator;
        memoryAllocatorStash.restore();
    }
};

TEST(TestMemoryAllocatorTest, SetCurrentNewAllocator)
{
    allocator = new TestMemoryAllocator("new allocator for test");
    setCurrentNewAllocator(allocator);
    POINTERS_EQUAL(allocator, getCurrentNewAllocator());
}

TEST(TestMemoryAllocatorTest, SetCurrentNewAllocatorToDefault)
{
    TestMemoryAllocator* originalAllocator = getCurrentNewAllocator();

    setCurrentNewAllocatorToDefault();
    POINTERS_EQUAL(defaultNewAllocator(), getCurrentNewAllocator());

    setCurrentNewAllocator(originalAllocator);
}

TEST(TestMemoryAllocatorTest, SetCurrentNewArrayAllocator)
{
    allocator = new TestMemoryAllocator("new array allocator for test");
    setCurrentNewArrayAllocator(allocator);
    POINTERS_EQUAL(allocator, getCurrentNewArrayAllocator());
    setCurrentNewArrayAllocatorToDefault();
    POINTERS_EQUAL(defaultNewArrayAllocator(), getCurrentNewArrayAllocator());
}

TEST(TestMemoryAllocatorTest, SetCurrentMallocAllocator)
{
    allocator = new TestMemoryAllocator("malloc_allocator");
    setCurrentMallocAllocator(allocator);
    POINTERS_EQUAL(allocator, getCurrentMallocAllocator());
    setCurrentMallocAllocatorToDefault();
    POINTERS_EQUAL(defaultMallocAllocator(), getCurrentMallocAllocator());
}

TEST(TestMemoryAllocatorTest, MemoryAllocation)
{
    allocator = new TestMemoryAllocator();
    allocator->free_memory(allocator->alloc_memory(100, "file", 1), "file", 1);
}

TEST(TestMemoryAllocatorTest, MallocNames)
{
    STRCMP_EQUAL("Standard Malloc Allocator", defaultMallocAllocator()->name());
    STRCMP_EQUAL("malloc", defaultMallocAllocator()->alloc_name());
    STRCMP_EQUAL("free", defaultMallocAllocator()->free_name());
}

TEST(TestMemoryAllocatorTest, NewNames)
{
    STRCMP_EQUAL("Standard New Allocator", defaultNewAllocator()->name());
    STRCMP_EQUAL("new", defaultNewAllocator()->alloc_name());
    STRCMP_EQUAL("delete", defaultNewAllocator()->free_name());
}

TEST(TestMemoryAllocatorTest, NewArrayNames)
{
    STRCMP_EQUAL("Standard New [] Allocator", defaultNewArrayAllocator()->name());
    STRCMP_EQUAL("new []", defaultNewArrayAllocator()->alloc_name());
    STRCMP_EQUAL("delete []", defaultNewArrayAllocator()->free_name());
}

TEST(TestMemoryAllocatorTest, NullUnknownAllocation)
{
    allocator = new NullUnknownAllocator;
    allocator->free_memory(allocator->alloc_memory(100, "file", 1), "file", 1);
}

TEST(TestMemoryAllocatorTest, NullUnknownNames)
{
    allocator = new NullUnknownAllocator;
    STRCMP_EQUAL("Null Allocator", allocator->name());
    STRCMP_EQUAL("unknown", allocator->alloc_name());
    STRCMP_EQUAL("unknown", allocator->free_name());
}

#if (! CPPUTEST_SANITIZE_ADDRESS)

#define MAX_SIZE_FOR_ALLOC ((size_t) -1 > (unsigned short)-1) ? (size_t) -97 : (size_t) -1

static void failTryingToAllocateTooMuchMemory(void)
{
    TestMemoryAllocator allocator;
    allocator.alloc_memory(MAX_SIZE_FOR_ALLOC, "file", 1);
} // LCOV_EXCL_LINE

TEST(TestMemoryAllocatorTest, TryingToAllocateTooMuchFailsTest)
{
    TestTestingFixture fixture;
    fixture.setTestFunction(&failTryingToAllocateTooMuchMemory);
    fixture.runAllTests();
    fixture.assertPrintContains("malloc returned null pointer");
}

#endif

#if CPPUTEST_USE_MEM_LEAK_DETECTION
#if CPPUTEST_USE_MALLOC_MACROS

class FailableMemoryAllocatorExecFunction : public ExecFunction
{
public:
    FailableMemoryAllocator* allocator_;
    void (*testFunction_)(FailableMemoryAllocator*);

    void exec() _override
    {
        testFunction_(allocator_);
    }

    FailableMemoryAllocatorExecFunction(FailableMemoryAllocator* allocator) : allocator_(allocator) {}
    virtual ~FailableMemoryAllocatorExecFunction() _destructor_override {}
};

TEST_GROUP(FailableMemoryAllocator)
{
    FailableMemoryAllocator *failableMallocAllocator;
    FailableMemoryAllocatorExecFunction * testFunction;
    TestTestingFixture *fixture;

    void setup()
    {
        fixture = new TestTestingFixture;
        failableMallocAllocator = new FailableMemoryAllocator("Failable Malloc Allocator", "malloc", "free");
        testFunction = new FailableMemoryAllocatorExecFunction(failableMallocAllocator);
        fixture->setTestFunction(testFunction);
        setCurrentMallocAllocator(failableMallocAllocator);
    }
    void teardown()
    {
        failableMallocAllocator->checkAllFailedAllocsWereDone();
        setCurrentMallocAllocatorToDefault();
        failableMallocAllocator->clearFailedAllocs();
        delete testFunction;
        delete failableMallocAllocator;
        delete fixture;
    }
};

TEST(FailableMemoryAllocator, MallocWorksNormallyIfNotAskedToFail)
{
    int *memory = (int*)malloc(sizeof(int));
    CHECK(memory != NULLPTR);
    free(memory);
}

TEST(FailableMemoryAllocator, FailFirstMalloc)
{
    failableMallocAllocator->failAllocNumber(1);
    POINTERS_EQUAL(NULLPTR, (int*)malloc(sizeof(int)));
}

TEST(FailableMemoryAllocator, FailSecondAndFourthMalloc)
{
    failableMallocAllocator->failAllocNumber(2);
    failableMallocAllocator->failAllocNumber(4);
    int *memory1 = (int*)malloc(sizeof(int));
    int *memory2 = (int*)malloc(sizeof(int));
    int *memory3 = (int*)malloc(sizeof(int));
    int *memory4 = (int*)malloc(sizeof(int));

    CHECK(NULLPTR != memory1);
    POINTERS_EQUAL(NULLPTR, memory2);
    CHECK(NULLPTR != memory3);
    POINTERS_EQUAL(NULLPTR, memory4);

    free(memory1);
    free(memory3);
}

static void _failingAllocIsNeverDone(FailableMemoryAllocator* failableMallocAllocator)
{
    failableMallocAllocator->failAllocNumber(1);
    failableMallocAllocator->failAllocNumber(2);
    failableMallocAllocator->failAllocNumber(3);
    malloc(sizeof(int));
    malloc(sizeof(int));
    failableMallocAllocator->checkAllFailedAllocsWereDone();
}

TEST(FailableMemoryAllocator, CheckAllFailingAllocsWereDone)
{
    testFunction->testFunction_ = _failingAllocIsNeverDone;

    fixture->runAllTests();

    LONGS_EQUAL(1, fixture->getFailureCount());
    fixture->assertPrintContains("Expected allocation number 3 was never done");
    failableMallocAllocator->clearFailedAllocs();
}

TEST(FailableMemoryAllocator, FailFirstAllocationAtGivenLine)
{
    failableMallocAllocator->failNthAllocAt(1, __FILE__, __LINE__ + 2);

    POINTERS_EQUAL(NULLPTR, malloc(sizeof(int)));
}

TEST(FailableMemoryAllocator, FailThirdAllocationAtGivenLine)
{
    int *memory[10] = { NULLPTR };
    int allocation;
    failableMallocAllocator->failNthAllocAt(3, __FILE__, __LINE__ + 4);

    for (allocation = 1; allocation <= 10; allocation++)
    {
        memory[allocation - 1] = (int *)malloc(sizeof(int));
        if (memory[allocation - 1] == NULLPTR)
            break;
        free(memory[allocation -1]);
    }

    LONGS_EQUAL(3, allocation);
}

static void _failingLocationAllocIsNeverDone(FailableMemoryAllocator* failableMallocAllocator)
{
    failableMallocAllocator->failNthAllocAt(1, "TestMemoryAllocatorTest.cpp", __LINE__);
    failableMallocAllocator->checkAllFailedAllocsWereDone();
}

TEST(FailableMemoryAllocator, CheckAllFailingLocationAllocsWereDone)
{
    testFunction->testFunction_ = _failingLocationAllocIsNeverDone;

    fixture->runAllTests();

    LONGS_EQUAL(1, fixture->getFailureCount());
    fixture->assertPrintContains("Expected failing alloc at TestMemoryAllocatorTest.cpp:");
    fixture->assertPrintContains("was never done");

    failableMallocAllocator->clearFailedAllocs();
}

#endif
#endif

TEST_GROUP(TestMemoryAccountant)
{
    MemoryAccountant accountant;

    void teardown()
    {
        accountant.clear();
    }
};

TEST(TestMemoryAccountant, totalAllocsIsZero)
{
    LONGS_EQUAL(0, accountant.totalAllocations());
    LONGS_EQUAL(0, accountant.totalDeallocations());
}

TEST(TestMemoryAccountant, countAllocationsPerSize)
{
    accountant.alloc(4);
    LONGS_EQUAL(1, accountant.totalAllocationsOfSize(4));
    LONGS_EQUAL(0, accountant.totalAllocationsOfSize(10));
    LONGS_EQUAL(1, accountant.totalAllocations());
}

TEST(TestMemoryAccountant, countAllocationsPerSizeMultipleAllocations)
{
    accountant.alloc(4);
    accountant.alloc(4);
    accountant.alloc(8);
    LONGS_EQUAL(2, accountant.totalAllocationsOfSize(4));
    LONGS_EQUAL(1, accountant.totalAllocationsOfSize(8));
    LONGS_EQUAL(0, accountant.totalAllocationsOfSize(10));
    LONGS_EQUAL(3, accountant.totalAllocations());
}

TEST(TestMemoryAccountant, countAllocationsPerSizeMultipleAllocationsOutOfOrder)
{
    accountant.alloc(4);
    accountant.alloc(8);
    accountant.alloc(4);
    accountant.alloc(5);
    accountant.alloc(2);
    accountant.alloc(4);
    accountant.alloc(10);

    LONGS_EQUAL(3, accountant.totalAllocationsOfSize(4));
    LONGS_EQUAL(1, accountant.totalAllocationsOfSize(8));
    LONGS_EQUAL(1, accountant.totalAllocationsOfSize(10));
    LONGS_EQUAL(7, accountant.totalAllocations());
}

TEST(TestMemoryAccountant, countDeallocationsPerSizeMultipleAllocations)
{
    accountant.dealloc(8);
    accountant.dealloc(4);
    accountant.dealloc(8);
    LONGS_EQUAL(1, accountant.totalDeallocationsOfSize(4));
    LONGS_EQUAL(2, accountant.totalDeallocationsOfSize(8));
    LONGS_EQUAL(0, accountant.totalDeallocationsOfSize(20));
    LONGS_EQUAL(3, accountant.totalDeallocations());
}

TEST(TestMemoryAccountant, countMaximumAllocationsAtATime)
{
    accountant.alloc(4);
    accountant.alloc(4);
    accountant.dealloc(4);
    accountant.dealloc(4);
    accountant.alloc(4);
    LONGS_EQUAL(2, accountant.maximumAllocationAtATimeOfSize(4));
}

TEST(TestMemoryAccountant, reportNoAllocations)
{
    STRCMP_EQUAL("CppUTest Memory Accountant has not noticed any allocations or deallocations. Sorry\n", accountant.report().asCharString());
}

TEST(TestMemoryAccountant, reportAllocations)
{
    accountant.dealloc(8);
    accountant.dealloc(8);
    accountant.dealloc(8);

    accountant.alloc(4);
    accountant.dealloc(4);
    accountant.alloc(4);
    STRCMP_EQUAL("CppUTest Memory Accountant report:\n"
                 "Allocation size     # allocations    # deallocations   max # allocations at one time\n"
                 "    4                   2                1                 1\n"
                 "    8                   0                3                 0\n"
                 "   Thank you for your business\n"
                 , accountant.report().asCharString());
}

TEST_GROUP(AccountingTestMemoryAllocator)
{
    MemoryAccountant accountant;
    AccountingTestMemoryAllocator *allocator;

    void setup()
    {
        allocator = new AccountingTestMemoryAllocator(accountant, getCurrentMallocAllocator());
    }

    void teardown()
    {
        accountant.clear();
        delete allocator;
    }
};

TEST(AccountingTestMemoryAllocator, canAllocateAndAccountMemory)
{
    char* memory = allocator->alloc_memory(10, __FILE__, __LINE__);
    allocator->free_memory(memory, __FILE__, __LINE__);

    LONGS_EQUAL(1, accountant.totalAllocationsOfSize(10));
    LONGS_EQUAL(1, accountant.totalDeallocationsOfSize(10));
}

TEST(AccountingTestMemoryAllocator, canAllocateAndAccountMemoryMultipleAllocations)
{
    char* memory1 = allocator->alloc_memory(10, __FILE__, __LINE__);
    char* memory2 = allocator->alloc_memory(8, __FILE__, __LINE__);
    char* memory3 = allocator->alloc_memory(12, __FILE__, __LINE__);

    allocator->free_memory(memory1, __FILE__, __LINE__);
    allocator->free_memory(memory3, __FILE__, __LINE__);

    char* memory4 = allocator->alloc_memory(15, __FILE__, __LINE__);
    char* memory5 = allocator->alloc_memory(20, __FILE__, __LINE__);

    allocator->free_memory(memory2, __FILE__, __LINE__);
    allocator->free_memory(memory4, __FILE__, __LINE__);
    allocator->free_memory(memory5, __FILE__, __LINE__);

    char* memory6 = allocator->alloc_memory(1, __FILE__, __LINE__);
    char* memory7 = allocator->alloc_memory(100, __FILE__, __LINE__);

    allocator->free_memory(memory6, __FILE__, __LINE__);
    allocator->free_memory(memory7, __FILE__, __LINE__);

    LONGS_EQUAL(7, accountant.totalAllocations());
    LONGS_EQUAL(7, accountant.totalDeallocations());
}

TEST(AccountingTestMemoryAllocator, useOriginalAllocatorWhenDeallocatingMemoryNotAllocatedByAllocator)
{
    char* memory = getCurrentMallocAllocator()->alloc_memory(10, __FILE__, __LINE__);
    allocator->free_memory(memory, __FILE__, __LINE__);
}

class GlobalMemoryAccountantExecFunction
    : public ExecFunction
{
public:
    virtual ~GlobalMemoryAccountantExecFunction() _destructor_override
    {
    }

    void (*testFunction_)(GlobalMemoryAccountant*);
    GlobalMemoryAccountant* parameter_;

    virtual void exec() _override
    {
        testFunction_(parameter_);
    }
};

TEST_GROUP(GlobalMemoryAccountant)
{
    GlobalMemoryAccountant accountant;
    TestTestingFixture fixture;
    GlobalMemoryAccountantExecFunction testFunction;

    void setup()
    {
        testFunction.parameter_ = &accountant;
        fixture.setTestFunction(&testFunction);
    }
};

TEST(GlobalMemoryAccountant, start)
{
    accountant.start();

    POINTERS_EQUAL(accountant.getMallocAllocator(), getCurrentMallocAllocator());
    POINTERS_EQUAL(accountant.getNewAllocator(), getCurrentNewAllocator());
    POINTERS_EQUAL(accountant.getNewArrayAllocator(), getCurrentNewArrayAllocator());

    accountant.stop();
}

TEST(GlobalMemoryAccountant, stop)
{
    TestMemoryAllocator* originalMallocAllocator = getCurrentMallocAllocator();
    TestMemoryAllocator* originalNewAllocator = getCurrentNewAllocator();
    TestMemoryAllocator* originalNewArrayAllocator = getCurrentNewArrayAllocator();

    accountant.start();
    accountant.stop();

    POINTERS_EQUAL(originalMallocAllocator, getCurrentMallocAllocator());
    POINTERS_EQUAL(originalNewAllocator, getCurrentNewAllocator());
    POINTERS_EQUAL(originalNewArrayAllocator, getCurrentNewArrayAllocator());
}

#if CPPUTEST_USE_MEM_LEAK_DETECTION

TEST(GlobalMemoryAccountant, report)
{
    accountant.start();
    char* memory = new char[185];
    delete [] memory;
    accountant.stop();

    /* Allocation includes memory leak info */
    STRCMP_CONTAINS("256", accountant.report().asCharString());
}

#endif

static void _failStopWithoutStartingWillFail(GlobalMemoryAccountant* accountant)
{
    accountant->stop();
}

TEST(GlobalMemoryAccountant, StopCantBeCalledWithoutStarting)
{
    testFunction.testFunction_ = _failStopWithoutStartingWillFail;
    fixture.runAllTests();
    fixture.assertPrintContains("GlobalMemoryAccount: Stop called without starting");
}

static void _failStartingTwiceWillFail(GlobalMemoryAccountant* accountant)
{
    accountant->start();
}

TEST(GlobalMemoryAccountant, startTwiceWillFail)
{
    testFunction.testFunction_ = _failStartingTwiceWillFail;
    fixture.runAllTests();
    accountant.stop();

    fixture.assertPrintContains("Global allocator start called twice!");

}

#if 0
TEST(GlobalMemoryAccountant, startTwiceWillFailWhenTheAllocatorIsntTheSame)
{
}

static void _failChangeMallocMemoryAllocator(GlobalMemoryAccountant* accountant)
{
    accountant->start();
    setCurrentMallocAllocator(defaultMallocAllocator());
    accountant->stop();
}

TEST(GlobalMemoryAccountant, checkWhetherMallocAllocatorIsNotChanged)
{
    testFunction.testFunction_ = _failChangeMallocMemoryAllocator;
    fixture.runAllTests();
    fixture.assertPrintContains("Something wrong: Malloc memory allocator has been changed while accounting for memory");
}


#endif
