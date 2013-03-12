if(MPI_FOUND)
  set(MPITESTS pamgtest galerkintest hierarchytest transfertest)
endif(MPI_FOUND)

if(ParMETIS_FOUND)
  set(PARMETISTESTS pamg_comm_repart_test)
endif(ParMETIS_FOUND)

set(NORMALTESTS amgtest fastamg graphtest kamgtest)
set(ALLTESTS ${MPITESTS} ${PARMETISTESTS} ${NORMALTESTS})

# We do not want want to build the tests during make all,
# but just build them on demand
add_directory_test_target(_test_target)
add_dependencies(${_test_target} ${ALLTESTS})

add_executable(amgtest "amgtest.cc")
add_dune_superlu_flags(amgtest)
add_executable(fastamg "fastamg.cc")
add_dune_superlu_flags(amgtest)
add_executable(galerkintest "galerkintest.cc")
add_executable(graphtest "graphtest.cc")
add_executable(hierarchytest "hierarchytest.cc")
add_executable(kamgtest "kamgtest.cc")
add_dune_superlu_flags(kamgtest)
add_executable(pamg_comm_repart_test "parallelamgtest.cc")
set_target_properties(pamg_comm_repart_test PROPERTIES COMPILE_FLAGS "-DAMG_REPART_ON_COMM_GRAPH")
add_executable(pamgtest "parallelamgtest.cc")
add_dune_superlu_flags(pamgtest)
add_executable(transfertest "transfertest.cc")

foreach(_exe ${ALLTESTS})
  target_link_libraries(${_exe} "dunecommon")
  add_test(${_exe} ${_exe})
endforeach(_exe ${ALLTESTS})

add_dune_mpi_flags("${MPITESTS}")
add_dune_parmetis_flags("${PARMETISTESTS}" pamgtest)