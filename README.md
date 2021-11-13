# CS-453 - Course project

The [project description](https://dcl.epfl.ch/site/_media/education/ca-project.pdf) is available on [Moodle](https://moodle.epfl.ch/course/view.php?id=14334) and the [website of the course](https://dcl.epfl.ch/site/education/ca_2021).

The description includes:
* an introduction to (software) transactional memory
* an introduction to concurrent programming in C11/C++11, with pointers to more resources
* the _specifications_ of the transactional memory you have to implement, i.e. both:
  * sufficient properties for a transactional memory to be deemed _correct_
  * a thorough description of the transactional memory interface
* practical informations, including:
  * how to test your implementation on your local machine and on the evaluation server
  * how your submission will be graded
  * rules for (optionally) using 3rd-party libraries and collaboration (although the project is _individual_)

This repository provides:
* examples of how to use synchronization primitives (in `sync-examples/`)
* a reference implementation (in `reference/`)
* a "skeleton" implementation (in `template/`)
  * this template is written in C11
  * feel free to overwrite it completely if you prefer to use C++ (in this case include `<tm.hpp>` instead of `<tm.h>`)
* the program that will test your implementation (in `grading/`)
  * the same program will be used on the evaluation server (although possibly with a different seed)
  * you can use it to test/debug your implementation on your local machine (see the [description](https://dcl.epfl.ch/site/_media/education/ca-project.pdf))
* a tool to submit your implementation (in `submit.py`)
  * you should have received by mail a secret _unique user identifier_ (UUID)
  * see the [description](https://dcl.epfl.ch/site/_media/education/ca-project.pdf) for more information
