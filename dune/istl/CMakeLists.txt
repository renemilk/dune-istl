add_subdirectory("paamg")
add_subdirectory("tutorial")
add_subdirectory("test" EXCLUDE_FROM_ALL)

#install headers
install(FILES
   basearray.hh
   bcrsmatrix.hh
   bdmatrix.hh
   btdmatrix.hh
   bvector.hh
   diagonalmatrix.hh
   gsetc.hh
   ilu.hh
   ilusubdomainsolver.hh
   io.hh
   istlexception.hh
   matrix.hh
   matrixindexset.hh
   matrixmarket.hh
   matrixmatrix.hh
   matrixredistribute.hh
   matrixutils.hh
   multitypeblockmatrix.hh
   multitypeblockvector.hh
   novlpschwarz.hh
   operators.hh
   overlappingschwarz.hh
   owneroverlapcopy.hh
   pardiso.hh
   preconditioner.hh
   preconditioners.hh
   repartition.hh
   scalarproducts.hh
   scaledidmatrix.hh
   schwarz.hh
   solvercategory.hh
   solver.hh
   solvers.hh
   solvertype.hh
   superlu.hh
   supermatrix.hh
   vbvector.hh
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dune/istl)
