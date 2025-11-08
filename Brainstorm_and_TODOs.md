# Brainstorm to add data reduction to DTLMod
## Assumptions
- Only one **reduction method** can be applied to a Variable **at a given time**.
- It should be possible to **change** the reduction method or its parameterization from **one transaction to another**.
- If we want to apply **different reduction methods** to the same Variable, it must be over **different Streams**.
  - in that case, we define a Variable for each stream (with the same name, shape, and distribution) and then apply a single reduction method at a time.
- A reduction method should be applied to a Variable either on the Publisher or the Subscriber side.
- As long as a reduction method is defined for a Variable, it must applied to each transaction that involves this Variable

## Notes on ADIOS does manage Variable reduction
- Has an Operator class (broader definition than just reduction, can also be encryption for instance)
- Has IO::defineOperator(name, type, parameters) to configure the operation for that IO (a.k.a. Stream in the DTL)
  - This function must be called when parsing the configuration file
- Has IO::addOperation(variable, operatorType, parameters) and Variable::addOperation(Operator, parameters) the former is to define the operation before the variable while the latter does the work
  - multiple operations can be applied to a single variable
- Has Variable::removeOperation
- The doc says that the same operation can be applied to "a set of variables", but nothing in the code looks like that.
- In the ADIOS XML configuration file, uses a <operator type="name"> and a set of key/value parameters (which provides a uniform way to parse), the name refers (mostly) to a specific lossy compressor. In the case of the DTL, it will rather be the name of a technique (e.g., decimation, compression, refactoring) all gather under "reduction" rather than "operator" 

## Considered Reduction Methods

### Decimation
- This method amounts to ignore some elements of the variables, i.e., *one every other X* in each dimension.
- It is parameterized by:
  - The applied **stride** that can be different for each dimension, e.g, {X, Y, Z} for a 3D variable
  - A **cost per element**. By default, as we have to simulate a traversal of the data, we can account for a couple flops per element (**e.g., 1 or 2**).
  - An optional **interpolation technique**
    - It simulates the fact that the elements kept capture information about the discarded elements. This would **increase the simulated cost per element** associated to the decimation.
    - Depending on the **shape** of the variable (i.e., number of dimensions), interpolation methods can for instance be **linear**, **quadatric**, or **cubic** ,referring to how many neighbors are considered.
    - **Assumptions for the first implementation:**
      - We do not consider interpolation beyond three dimensions
      - We do not consider the fact that elements needed by a rank to compute the interpolation are owned by another rank.
- Impact on the local and global sizes of the variable:
  - For each of its dimensions, a Variable is defined by a *shape* (the total #elements) and two arrays of *local_start* (the offset for each actor owning a part of the data) and *local_count* (the number of elements owned by each actor) values. When we apply a decimation of stride *X* on a dimension, we have:
    - **reduced_shape** = ceil(shape / (1.0 * X ))
    - **reduced_local_start** = ceil(local_start / (1.0 * X )) and 0 if greater than (reduced_shape - 1)
    - **reduced_local_count** = min(shape, ceil((local_start + local_count) / 1.0 * X)) - ceil(local_start / (1.0 * X)) 
  - The `get_reduced_global_size()` method will return the product of the element size by the `reduced_shape` of each dimension.
  - The `get_reduced_local_size()` method will return the product of the `reduced_local_count` of each dimension.
  - **Note:** these two functions may be for internal purposes only and not user-facing.

- **Behavior depending on the engine type**
  - With a **File** engine
    - If the reduction method is applied on the **publisher side**, it's to speed up I/O and reduce the storage footprint (e.g., for a checkpoint operation). A `put()` of a Variable on which decimation is applied considers the **local_reduced_size** for the I/O operations triggered by the put. This is automatically reflected in the metadata stored for this Variable.
    - If the reduction method is applied on the **subscriber side**, this means that the subscriber does not want to fetch the entire data from storage. The behavior is thus similar to that of calling `set_selection()`.    
  - With a **Staging** engine
    - The reduction method can be applied on either side of the stream with the same effect. The internal behavior is close to that of a transaction with a selection. The exact data transfer pattern is determined when the subscribers specify what they need from the publishers. In that case, this means to consider the **reduced** versions of **shape**, **start**, and **count** to determine the block to exchange. A notable difference is that if the reduction method is applied on one side, the other side must compute the reduced information first.   
    - When the decimation does interpolation, the cost of its computation must be simulated on the publisher side, but only when determining the exchanges between individual pairs of publishers and subscribers.


### Compression
- This methods produces a smaller version of a variable on the publisher side, but keep the same metadata information (shape, start, and, count).
- It is parameterized by:
  - An **accuracy** (usually expressed as 10 to the minus X). The lower the accuracy value (meaning more decimals must be kept), the lower the compression ratio, and the lower the compression cost.
  - A **compression cost per element** applied on the **publisher side**
  - A **decompression cost per element** applied on the **subscriber side**
- The overall reduction cost and the size of the reduced version of the variable (or compression ratio) are related to the **accuracy** 
- The compression and decompression costs are is likely to be much higher than for decimation, but still are proportional to the number of elements in the variable (must consider each and every element), which motivated the use of a cost per element as a first approximation. This will also allow us to distinguish executions on CPU or GPU at some point (in the later version).

- **Behavior depending on the engine type**
  - With a **File** engine
    - The initial definition of the variable is kept (as decompressing will come back to that) but the local/global sizes are impacted
    - Add a `local_compressed_size` and a `global_compressed_size` (uniform across ranks or not is to be decided)
       Use the `local_compressed_size` in the `put()` and when transforming the `put()` into I/O operations
        - The variable must also be tagged as `stored_compressed`, likely with extra details about the compression technique, ratio, ...
  - With a **Staging** engine
    - Here reduction only aims at speeding up data transport as data is not stored. Blocks to exchange are computed based on the original description of the data, but the size of each block is reduced to be transferred
      - **Note:** as a first approximation we assume a **uniform** compression of each block.  
  - In both cases, the compression happens at the beginning of the `put()` on the publisher side, while decompression happens at the end of the `get()` on the subscriber side.


## Proposed API and behavior (WIP)
  - [x] new `ReductionMethod` class: An abstract class from which decimation and compression will inherit
    - members:
      - [x] `std::string name_`
    - methods and behavior:
      - [x] `get_name` and `get_cname`
      - [x] `virtual void parameterize_for_variable(std::shared_ptr<Variable> var, std::map<std::string, std::string> parameters) = 0`: parse the parameters and creates the parameterized version of the `ReductionMethod`
      - [x] `virtual void reduce_variable(std::shared_ptr<Variable> var) = 0`: compute `reduced_shape` and `reduced_local_start_and_count` for a variable and store them in the `ParameterizedDecimation`
      - [x] `virtual size_t get_reduced_variable_global_size(std::shared_ptr<Variable> var) const = 0`
      - [x] `virtual size_t get_reduced_variable_local_size(std::shared_ptr<Variable> var) const = 0`

  - [ ] new `DecimationReductionMethod` class
    - members:
      - [x] `std::map<std::shared_ptr<Variable>, ParameterizedDecimation> per_variable_parameterizations_`
    - methods and behavior:
      - The parameters used by the decimation method can be different for each variable. They must be stored in a map whose key is the Variable (assuming that the a `ReductionMethod` can only be applied once to a variable). The values in that map are A `ParameterizedDecimation` objects that contain the `stride`, `interpolation_method`, and `cost_per_element` to use for this variable.
      - The parameterization to use for a variable is set when calling `Variable::add_reduction_operation`

   - [ ] new `ParameterizedDecimation` class
      - members:
        - [x] `std::vector<size_t> stride_`
        - [x] `std::string interpolation_method_` (default to empty string)
        - [x] `double cost_per_element_`
        - [x] `std::vector<size_t> reduced_shape_`
        - [x] `std::unordered_map<sg4::ActorPtr, std::pair<std::vector<size_t>, std::vector<size_t>>> reduced_local_start_and_count_`
        - [x] `size_t element_size_`
      - methods and behavior:


  - [ ] new `CompressionReductionMethod` class
    - members:
      - [x] `std::map<std::shared_ptr<Variable>, ParameterizedCompression> per_variable_parameterizations_`
    - methods:
      - [x] `void parse_parameters(std::shared_ptr<Variable> var, std::map<std::string, std::string> parameters)`
    - behavior:
        - The parameters used by the compression method can be different for each variable. They must be stored in a map whose key is the Variable (assuming that the a `ReductionMethod` can only be applied once to a variable). The values in that map are A `ParameterizedCompression` objects that contain the `accuracy`, `compression_cost_per_element`, and `decompression_cost_per_element` to use for this variable.
        - The parameterization to use for a variable is set when calling `Variable::add_reduction_operation`

  - [ ] New member(s) and function(s) in `Stream` class
    - members:
      - [x] `reduction_methods_`, a vector of `ReductionMethod` objects
    - methods:
      - [x]`std::shared_ptr<ReductionMethod> Stream::define_reduction_method(const std::string& name)` to create a new `ReductionMethod` object and store it in `reduction_methods_`

  - [ ] New member(s) and function(s) in `Variable` class
    - members:
      - [x] `is_reduced_with_`: the shared pointer to the applied `ReductionMethod`, set by `set_reduction_operation`
    - methods:
      - [x] `void Variable::set_reduction_operation(std::shared_ptr<ReductionMethod>, std::map<std::string, std::string> parameters)` that triggers the parameter parsing of the `ReductionMethod` passed in argument
      - [x] `bool is_reduced() const`
        - [ ] decide if public or protected
      - [x] `const std::shared_ptr<ReductionMethod>& get_reduction_method() const`
        - [ ] decide if public or protected


## TODOs
- [ ] add tests in `test/dtl_reduction.cpp`
  - [ ] SimpleDecimationFileEngine:
- [ ] add python binding
- [ ] add documentation