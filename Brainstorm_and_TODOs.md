# Brainstorm and TODOs to add data reduction to DTLMod

## Brainstorm
- Need to be able to attach a **reduction** technique to each **variable** published to the DTL

- Consider the following techniques to start with:
  - **Simple decimation:** This simply amounts to ignore some elements of the variables, likely *one every other X* in each dimension.
    - The applied *stride* can be different for each dimension, e.g, {X, Y, Z} for a 3D variable
    - Implications:
        - Reduces the local and global size by X * Y * Z (roughly) on the Publisher side
        - Does not cost a lot of computation, but still have to simulate a traversal of the data. A few flops per element will do.
  - **Decimation with interpolation:** Similar technique but in addition to the stride, we simulate the fact that the elements kept capture information about the discarded elements. This would increase the simulated cost associated to the decimation.
    - Depending on the shape of the variable (i.e., number of dimensions), interpolation methods can for instance be *linear*, *quadatric*, or *cubic*, referring to how many neighbors are considered.
    - Not sure it is worth considering, in a first implementation, elements that are needed by a rank to for compute the interpolation but are owned by another rank.
  - **Lossy compression:** With compression, the simulated cost to reduce the size of a variable on the publisher side  is likely to be much higher than for decimation. Moreover, this cost and the size of the reduced version of the variable (or compression ratio) is related to an *accuracy* (usually expressed as 10 to the minus X). The lower the accuracy value (meaning more decimals must be kept), the lower the compression ratio, and the lower the compression cost. Finally, a cost to decompress the variable must also be applied on the subscriber side. Both compression and decompression costs are proportional to the number of elements in the variable (must consider each and every element), hence we can use a cost per element as a first approximation. This can also allow us to distinguish executions on CPU or GPU at some point. 

- Have to distinguish the behavior depending on either we use File-based or Staging engines and on the reduction method:
    - File-based: If data is reduced on the publishing side, it's to speed up I/O and reduce the storage footprint. Then, the raw version of the (data) is not kept.
        - With decimation, the initial definition of the variable is changed (less elements per dimension)
        - With compression, the initial definition of the variable is kept (as decompressing will come back to that) but the local/global sizes are impacted
            - Add a local_compressed_size and a global_compressed_size (uniform across ranks or not is to be decided)
            - Use the local_compressed_size in the put() and when transforming the put() into I/O operations
            - The variable must also be tagged as "stored_compressed", likely with extra details about the compression technique, ratio, ...
        - the reduction operation has to be applied in every transaction (at the beginning of the put() on the publisher side, decompression happens at the end of the get())
    - Staging: here reduction only aims at speeding up data transport as data is not stored
        - With simple decimation, the reduction can be initiated by a subscriber. Specifying strides is another type of selection
        - When the decimation does interpolation, the cost of its computation must be simulated on the publisher side, but only when determining the exchanges between individual pairs of publishers and subscribers.
        - With compression, as for files, the reduction operation has to be applied in every transaction (at the beginning of the put() on the publisher side, decompression happens at the end of the get())       

- How does ADIOS manage a reduction method associated to a variable?
    - Has an Operator class (broader definition than just reduction, can also be encryption for instance)
    - Has IO::defineOperator(name, type, parameters) to configure the operation for that IO (a.k.a. Stream in the DTL)
        - This function must be called when parsing the configuration file
   - Has IO::addOperation(variable, operatorType, parameters) and Variable::addOperation(Operator, parameters) the former is to define the operation before the variable while the latter does the work
        - multiple operations can be applied to a single variable
   - Has Variable::removeOperation
   - The doc says that the same operation can be applied to "a set of variables", but nothing in the code looks like that.
   - In the AIDOS XML configuration file, uses a <operator type="name"> and a set of key/value parameters (which provides a uniform way to parse), the name refers (mostly) to a specific lossy compressor. In the case of the DTL, it will rather be the name of a technique (e.g., decimation, compression, refactoring) all gather under "reduction" rather than "operator" 

- Proposed API and behavior
    - [x] new `ReductionMethod` class
        - members:
            - [x] `std::string name_`
        - methods:
            - [x] `get_name` and `get_cname`
        - behavior:
            - An abstract class from which decimation and compression will inherit
    - [ ] new `DecimationReductionMethod` class
        - members:
            - [ ] 
        - methods:
            - [ ] `void parse_parameters(std::map<std::string, std::string> parameters)` 
        - behavior:
            - 
    - [ ]`std::shared_ptr<ReductionMethod> Stream::define_reduction_method(const std::string& name)`
        - behavior:
            - [ ] creates a new ReductionMethod object
            - [x] Implies to store a vector of ReductionMethods in the Stream object (in `reduction_methods_`)
    - `void Variable::add_reduction_operqtion(std::shared_ptr<ReductionMethod>, std::map<std::string, std::string> parameters)`
        - behavior:
            - [ ] parse `parameters`

## TODOs
- [ ] add tests
- [ ] add python binding
- [ ] add documentation